#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>

#define lottie1_path "C:/Users/XSE-R/Pictures/lottie/Black Cat Ball.json"
#define lottie2_path "C:/Users/XSE-R/Pictures/lottie/Arrow left.json"
#define lottie3_path "C:/Users/XSE-R/Pictures/lottie/black rainbow cat.json"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow window;
    window.resize(800, 600);
    window.setWindowTitle(QObject::tr("Qt + ThorVG"));

    // 設定主視窗的背景色，例如藍色
    window.setStyleSheet("background-color: blue;");

    // 檔案路徑由上方 #define 指定；在此設定各動畫在主視窗的位置與大小。
    auto *first = window.addLottie(QStringLiteral(lottie1_path), 16, 16, 380, 380);
    auto *second = window.addLottie(QStringLiteral(lottie2_path), 412, 16, 372, 380);

    if (first == nullptr && second == nullptr) {
        QMessageBox::warning(&window,
                             QObject::tr("ThorVG"),
                             QObject::tr("無法載入 Lottie 檔案。請檢查 lottie1_path / lottie2_path。"));
    }

    window.show();
    return a.exec();
}
