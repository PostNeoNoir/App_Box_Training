#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QTimer>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct SensorData {
    quint16 impactForce;
    quint16 reactionTime;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void sendCommand();
    void onReadyRead();
    void startSending();

    void on_start_training_Button_clicked();

    void on_create_training_Button_clicked();

    void on_fast_training_Button_clicked();

    void on_lampButton_clicked(int lampNumber); // Универсальный слот

private:
    Ui::MainWindow *ui;
    QUdpSocket *SocketUPD;
    QTimer *timer;
    int SendCount;
    int Port;
    QString accumulatedData;
    QString receiverIP;
    quint16 receiverPort;
    QVector<SensorData> sensorDataArray;
    bool recordingMode = false; // Флаг записи
    QList<int> trainingSequence; // Список последовательности
};

#endif // MAINWINDOW_H
