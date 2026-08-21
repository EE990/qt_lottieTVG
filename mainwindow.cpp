#include "mainwindow.h"
#include "thorvgwidget.h"
#include "ui_mainwindow.h"

#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupPlaybackButtons();
}

MainWindow::~MainWindow()
{
    // ThorVGWidget 是 centralwidget 的子物件，delete ui 時會一併解構並釋放 ThorVG 資源。
    m_views.clear();
    delete ui;
}

void MainWindow::setupPlaybackButtons()
{
    auto *pauseButton = new QPushButton(tr("暫停"), ui->centralwidget);
    pauseButton->setGeometry(16, 400, 80, 32);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::pauseAll);

    auto *playButton = new QPushButton(tr("播放"), ui->centralwidget);
    playButton->setGeometry(16, 440, 80, 32);
    connect(playButton, &QPushButton::clicked, this, &MainWindow::playAll);

    auto *replayButton = new QPushButton(tr("重播"), ui->centralwidget);
    replayButton->setGeometry(16, 480, 80, 32);
    connect(replayButton, &QPushButton::clicked, this, &MainWindow::replayAll);
}

ThorVGWidget *MainWindow::addLottie(const QString &path, int x, int y, int w, int h)
{
    auto *view = new ThorVGWidget(ui->centralwidget);
    view->setGeometry(x, y, w, h);

    if (!view->loadFile(path)) {
        delete view; // 載入失敗立即釋放，避免留下空 widget 與未用緩衝區
        return nullptr;
    }

    view->show();
    m_views.append(view);
    return view;
}

void MainWindow::playAll()
{
    for (auto *view : m_views)
        view->play();
}

void MainWindow::pauseAll()
{
    for (auto *view : m_views)
        view->pause();
}

void MainWindow::replayAll()
{
    for (auto *view : m_views)
        view->replay();
}
