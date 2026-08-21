#ifndef THORVGWIDGET_H
#define THORVGWIDGET_H

#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <memory>

// ThorVG 只在 .cpp 引入完整標頭，此處前向宣告即可。
namespace tvg {
class Animation;
class Picture;
class SwCanvas;
}

/**
 * Qt 與 ThorVG 的橋接元件。
 *
 * 職責切分：
 * - ThorVG：解析 Lottie、光栅化到 CPU 像素緩衝區
 * - Qt：提供視窗、計時、把緩衝區貼上螢幕
 * 兩邊不共用繪圖 API，只共用 m_buffer。
 *
 * 可同時建立多個實例；行程級引擎以參考計數共用，銷毀時各自釋放自己的 canvas / animation / buffer。
 */
class ThorVGWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ThorVGWidget(QWidget *parent = nullptr);
    ~ThorVGWidget() override;

    ThorVGWidget(const ThorVGWidget &) = delete;
    ThorVGWidget &operator=(const ThorVGWidget &) = delete;

    /// 從外部檔案路徑載入 Lottie（不走 Qt 資源系統）。重複呼叫會先釋放上一份場景。
    bool loadFile(const QString &path);
    QString sourcePath() const { return m_sourcePath; }

    void play();
    void pause();
    void replay();
    bool isPlaying() const { return m_playing; }

protected:
    /// 只負責把已光栅化的 m_buffer 畫到 widget，不做向量計算。
    void paintEvent(QPaintEvent *event) override;
    /// 尺寸變了必須重建 canvas target，舊緩衝區指標會失效。
    void resizeEvent(QResizeEvent *event) override;

private slots:
    /// 依牆上時鐘對齊動畫進度，再要求 ThorVG 畫下一幀。
    void updateAnimation();

private:
    bool ensureEngine();
    void releaseEngine();
    void releaseScene(bool releaseAnimation);
    bool setupCanvas();
    void fitPictureToWidget();
    void renderFrame();
    void seekElapsed(qint64 elapsedMs);
    void startPlaybackTimer();

    QString m_sourcePath;
    QTimer m_timer;             // 驅動播放；實際幀號由 m_clock 決定
    QElapsedTimer m_clock;      // 牆上時間，卡住時會跳幀而非越播越慢
    QVector<uint32_t> m_buffer; // ThorVG 寫入、QImage 讀取的共享像素

    std::unique_ptr<tvg::Animation> m_animation;
    std::unique_ptr<tvg::SwCanvas> m_canvas;
    tvg::Picture *m_picture = nullptr; // 由 Animation 擁有，不可 delete

    float m_totalFrame = 0.0f;
    float m_duration = 0.0f; // 秒
    qint64 m_originMs = 0;   // 暫停前已播放的毫秒，接續播放時加回去
    bool m_playing = false;
    bool m_engineHeld = false; // 本實例是否已取得引擎參考，避免未載入成功也去 term
};

#endif // THORVGWIDGET_H
