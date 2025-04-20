#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startSending();
    void sendCommand();
    void onReadyRead();
    void sendToFirebase(int lampNumber, int impact, float reactionTime);

    void on_start_training_Button_clicked();
    void on_create_training_Button_clicked();
    void on_fast_training_Button_clicked();
    void on_lampButton_clicked(int lampNumber);

private:
    Ui::MainWindow *ui;
    QUdpSocket *SocketUPD;
    QNetworkAccessManager *networkManager;

    QString receiverIP = "192.168.1.100";
    quint16 receiverPort = 8888;
    QString firebaseURL = "https://boxtrain-65522-default-rtdb.firebaseio.com/data.json";

    QString accumulatedData;
    QList<int> trainingSequence;
    bool recordingMode = false;
};

#endif // MAINWINDOW_H
