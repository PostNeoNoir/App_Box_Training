// =============================
// 📄 QT MainWindow.h для HTTP
// =============================
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QVector>
#include <QPainter>
#include <QTextEdit> // Добавьте в начале файла

#include "logindialog.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE



// Добавьте перед классом MainWindow
struct ZoneStats {
    int totalImpact = 0;
    double totalTime = 0.0;
    int count = 0;
};

struct ZoneAnalysis {
    QString message;
    bool needsAttention = false;
};

struct TrainingAnalysis {
    QVector<ZoneAnalysis> zones;
    QString overallMessage;
};

struct SensorData {
    quint16 impactForce;
    quint16 reactionTime;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void sendCommand();
    void startSending();
    void pollESP();

    void on_start_training_Button_clicked();
    void on_create_training_Button_clicked();
    void on_fast_training_Button_clicked();
    void on_lampButton_clicked(int lampNumber);

    void on_statistic_Button_clicked();

    void onStatisticDataReceived(QNetworkReply *reply);

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *networkManager;
    QTimer *pollTimer;

    int SendCount;
    QString username;

    QString accumulatedData;
    QString receiverIP;
    quint16 receiverPort;
    QVector<SensorData> sensorDataArray;
    bool recordingMode = false;
    QList<int> trainingSequence;

    void resetButtonTexts();

    void showStatisticWindow(const QString &statistics);

    QVector<QJsonObject> m_currentTrainings;

    void showStatisticWindowWithCharts(const QVector<QJsonObject> &trainings);
    bool eventFilter(QObject *watched, QEvent *event) override;
    void drawCharts(QPainter &painter, const QRect &rect);
    void drawBarChart(QPainter &painter, const QRect &rect,
                      const QVector<double> &values,
                      const QString &title, const QColor &color);
    void requestUserTypeAndStartTraining();
    void generateTrainingSequence(const QString &userType);

    TrainingAnalysis analyzeTraining(const QVector<QJsonObject> &trainings);

protected:
    void paintStats(QPainter &painter, const QVector<QJsonObject> &trainings);
};

#endif // MAINWINDOW_H
