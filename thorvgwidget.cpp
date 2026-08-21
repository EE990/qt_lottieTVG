#include "thorvgwidget.h"

#include <QDebug>
#include <QFile>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

#include <cmath>
#include <thorvg.h>

namespace {

// Initializer::init/term 是行程級的。每個成功 init 的 widget 各持一票，最後一票才 term。
int engineRefCount = 0;

const char *resultText(tvg::Result result)
{
    switch (result) {
    case tvg::Result::Success:
        return "Success";
    case tvg::Result::InvalidArguments:
        return "InvalidArguments";
    case tvg::Result::InsufficientCondition:
        return "InsufficientCondition";
    case tvg::Result::FailedAllocation:
        return "FailedAllocation";
    case tvg::Result::MemoryCorruption:
        return "MemoryCorruption";
    case tvg::Result::NonSupport:
        return "NonSupport";
    case tvg::Result::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

} // namespace

ThorVGWidget::ThorVGWidget(QWidget *parent)
    : QWidget(parent)
{
    // 整塊由 QImage 覆蓋，可略過 Qt 預設背景以減少閃爍。
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(1, 1);

    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &ThorVGWidget::updateAnimation);
}

ThorVGWidget::~ThorVGWidget()
{
    m_timer.stop();
    releaseScene(true);
    releaseEngine();
}

bool ThorVGWidget::ensureEngine()
{
    if (m_engineHeld)
        return true;

    if (engineRefCount == 0) {
        // Sw = CPU 軟體光栅器；threads=0 只在 Qt 主執行緒畫，方便與 paintEvent 同步。
        const auto result = tvg::Initializer::init(tvg::CanvasEngine::Sw, 0);
        if (result != tvg::Result::Success) {
            qWarning() << "ThorVG init failed:" << resultText(result);
            return false;
        }
    }

    ++engineRefCount;
    m_engineHeld = true;
    return true;
}

void ThorVGWidget::releaseEngine()
{
    if (!m_engineHeld)
        return;

    m_engineHeld = false;
    if (--engineRefCount == 0)
        tvg::Initializer::term(tvg::CanvasEngine::Sw);
}

void ThorVGWidget::releaseScene(bool releaseAnimation)
{
    if (m_canvas) {
        m_canvas->sync();
        // Picture 由 Animation 持有；clear(true) 會 double-free。
        m_canvas->clear(false);
        m_canvas.reset();
    }

    if (releaseAnimation) {
        m_picture = nullptr;
        m_animation.reset();
        m_buffer.clear();
        m_buffer.squeeze(); // 把容量還給系統，避免換檔後仍佔著舊緩衝區
        m_totalFrame = 0.0f;
        m_duration = 0.0f;
        m_originMs = 0;
        m_playing = false;
        m_sourcePath.clear();
    }
}

bool ThorVGWidget::loadFile(const QString &path)
{
    m_timer.stop();
    releaseScene(true);

    if (path.isEmpty()) {
        qWarning() << "Lottie path is empty";
        update();
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open Lottie file:" << path << file.errorString();
        update();
        return false;
    }

    if (!ensureEngine()) {
        update();
        return false;
    }

    const QByteArray bytes = file.readAll();
    file.close();

    m_animation = tvg::Animation::gen();
    m_picture = m_animation->picture();

    // 記憶體載入可避開 Windows 路徑編碼；copy=true 讓 ThorVG 自行保存，bytes 可立刻釋放。
    const auto loadResult = m_picture->load(bytes.constData(),
                                            static_cast<uint32_t>(bytes.size()),
                                            "lottie",
                                            true);
    if (loadResult != tvg::Result::Success) {
        qWarning() << "ThorVG failed to load" << path << ":" << resultText(loadResult);
        releaseScene(true);
        update();
        return false;
    }

    m_sourcePath = path;
    m_totalFrame = m_animation->totalFrame();
    m_duration = m_animation->duration();

    if (!setupCanvas()) {
        releaseScene(true);
        update();
        return false;
    }

    renderFrame();
    replay();
    return true;
}

bool ThorVGWidget::setupCanvas()
{
    const int w = qMax(1, width());
    const int h = qMax(1, height());

    // 只拆 canvas，保留 Animation / Picture，供 resize 後重新掛上。
    releaseScene(false);

    m_buffer.fill(0, w * h);
    m_canvas = tvg::SwCanvas::gen();
    // 多實例時各自記憶體池，避免共用 pool 互相覆蓋。
    m_canvas->mempool(tvg::SwCanvas::MempoolPolicy::Individual);
    // stride 以像素計（不是位元組）；ARGB8888 為預乘，對應 QImage::Format_ARGB32_Premultiplied。
    const auto targetResult = m_canvas->target(m_buffer.data(),
                                               static_cast<uint32_t>(w),
                                               static_cast<uint32_t>(w),
                                               static_cast<uint32_t>(h),
                                               tvg::SwCanvas::ARGB8888);
    if (targetResult != tvg::Result::Success) {
        qWarning() << "ThorVG canvas target failed:" << resultText(targetResult);
        releaseScene(false);
        return false;
    }

    if (m_picture) {
        fitPictureToWidget();
        // cast 把 raw pointer 包成 unique_ptr 交給 canvas 管理場景節點，所有權仍在 Animation。
        const auto pushResult = m_canvas->push(tvg::cast(m_picture));
        if (pushResult != tvg::Result::Success) {
            qWarning() << "ThorVG canvas push failed:" << resultText(pushResult);
            releaseScene(false);
            return false;
        }
    }

    return true;
}

void ThorVGWidget::fitPictureToWidget()
{
    if (!m_picture)
        return;

    float pictureW = 0.0f;
    float pictureH = 0.0f;
    m_picture->size(&pictureW, &pictureH);
    if (pictureW <= 0.0f || pictureH <= 0.0f)
        return;

    const float viewW = static_cast<float>(qMax(1, width()));
    const float viewH = static_cast<float>(qMax(1, height()));
    // 取較小邊等比例縮放並置中，避免變形。
    const float scale = qMin(viewW / pictureW, viewH / pictureH);

    m_picture->scale(scale);
    m_picture->translate((viewW - pictureW * scale) * 0.5f,
                         (viewH - pictureH * scale) * 0.5f);
}

void ThorVGWidget::seekElapsed(qint64 elapsedMs)
{
    if (!m_animation || m_duration <= 0.0f)
        return;

    const float elapsedSec = static_cast<float>(elapsedMs) / 1000.0f;
    float progress = std::fmod(elapsedSec, m_duration) / m_duration;
    if (progress < 0.0f)
        progress = 0.0f;

    m_animation->frame(progress * m_totalFrame);
    renderFrame();
}

void ThorVGWidget::startPlaybackTimer()
{
    if (m_duration <= 0.0f || m_totalFrame <= 1.0f)
        return;

    const float fps = m_totalFrame / m_duration;
    m_timer.start(qMax(8, qRound(1000.0f / fps)));
}

void ThorVGWidget::play()
{
    if (!m_animation || m_duration <= 0.0f || m_playing)
        return;

    m_playing = true;
    m_clock.restart();
    startPlaybackTimer();
}

void ThorVGWidget::pause()
{
    if (!m_playing)
        return;

    m_originMs += m_clock.elapsed();
    m_playing = false;
    m_timer.stop();
}

void ThorVGWidget::replay()
{
    if (!m_animation)
        return;

    m_originMs = 0;
    m_playing = false;
    m_timer.stop();
    seekElapsed(0);
    play();
}

void ThorVGWidget::renderFrame()
{
    if (!m_canvas)
        return;

    m_canvas->update(); // 準備繪圖指令（未寫像素）
    m_canvas->draw();   // 光栅化進 m_buffer
    m_canvas->sync();   // 非同步引擎也必須等到寫完
    update();           // 請 Qt 呼叫 paintEvent 貼圖
}

void ThorVGWidget::updateAnimation()
{
    if (!m_playing || !m_animation || !m_canvas || m_duration <= 0.0f)
        return;

    // 暫停時把已播時間存進 m_originMs，播放時只累加這一段 clock。
    seekElapsed(m_originMs + m_clock.elapsed());
}

void ThorVGWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    const int w = width();
    const int h = height();
    // 緩衝區與 widget 尺寸不一致時不讀，避免 resize 過程越界。
    if (!m_canvas || w <= 0 || h <= 0 || m_buffer.size() != w * h)
        return;

    // 不拷貝像素：QImage 只包裝 ThorVG 已寫好的緩衝區，生命週期僅限此次 paintEvent。
    QImage image(reinterpret_cast<const uchar *>(m_buffer.constData()),
                 w,
                 h,
                 w * static_cast<int>(sizeof(uint32_t)),
                 QImage::Format_ARGB32_Premultiplied);
    painter.drawImage(0, 0, image);
}

void ThorVGWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_animation || !m_picture)
        return;

    if (!setupCanvas())
        return;

    renderFrame();
}
