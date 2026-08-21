#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ThorVGWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /// 載入指定路徑的 Lottie，並放到主視窗指定位置與大小。
    /// 回傳建立的 widget；失敗為 nullptr（不會留下空元件）。
    ThorVGWidget *addLottie(const QString &path, int x, int y, int w, int h);

private slots:
    void playAll();
    void pauseAll();
    void replayAll();

private:
    void setupPlaybackButtons();

    Ui::MainWindow *ui;
    QList<ThorVGWidget *> m_views; // 非擁有指標；實際由 centralwidget 父子關係釋放
};
#endif // MAINWINDOW_H
