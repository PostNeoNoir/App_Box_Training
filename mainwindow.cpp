#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QtEndian>

#include <QSslSocket>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , receiverIP("192.168.1.100")
    , receiverPort(8888)
{
    ui->setupUi(this);

    // Инициализируем сетевой менеджер
    networkManager = new QNetworkAccessManager(this);

    // Привязываем кнопки к их обработчикам
    for (int i = 1; i <= 7; ++i) {
        QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(i));
        if (button) {
            connect(button, &QPushButton::clicked, this, [this, i]() {
                on_lampButton_clicked(i);
            });
        }
    }

    // Создаём UDP-сокет
    SocketUPD = new QUdpSocket(this);
    SocketUPD->bind(receiverPort);

    connect(SocketUPD, &QUdpSocket::readyRead, this, &MainWindow::onReadyRead);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Генерация и отправка случайной последовательности команд
void MainWindow::startSending()
{
    accumulatedData.clear();
    for (int i = 0; i < 10; ++i) {
        int randomValue = QRandomGenerator::global()->bounded(1, 8);
        accumulatedData.append(QString::number(randomValue));
    }

    sendCommand();
}

// Отправка данных на ESP32
void MainWindow::sendCommand()
{
    QByteArray datagram = accumulatedData.toUtf8();
    QHostAddress receiverAddress(receiverIP);
    SocketUPD->writeDatagram(datagram, receiverAddress, receiverPort);
    qDebug() << "Отправил: " << datagram;
    qDebug() << "SSL support available:" << QSslSocket::supportsSsl();
}

// Получение данных удара от ESP32
void MainWindow::onReadyRead()
{
    while (SocketUPD->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(SocketUPD->pendingDatagramSize());
        SocketUPD->readDatagram(datagram.data(), datagram.size());

        if (datagram.size() == 7) {
            uint8_t lampNumber;
            uint16_t impact;
            float reactionTime;

            // Распаковываем данные
            memcpy(&lampNumber, datagram.data(), sizeof(lampNumber));
            memcpy(&impact, datagram.data() + sizeof(lampNumber), sizeof(impact));
            memcpy(&reactionTime, datagram.data() + sizeof(lampNumber) + sizeof(impact), sizeof(reactionTime));

            qDebug() << "Лампа:" << lampNumber;
            qDebug() << "Сила удара:" << impact;
            qDebug() << "Время реакции:" << reactionTime;

            // Обновляем кнопку
            QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(lampNumber));
            if (button) {
                button->setText(QString("Сила удара: %1\nВремя: %2 сек")
                                    .arg(impact)
                                    .arg(reactionTime, 0, 'f', 2));
            }

            // Отправляем данные в Firebase
            sendToFirebase(lampNumber, impact, reactionTime);
        } else {
            qDebug() << "Неверный размер пакета!";
        }
    }
}

// Отправка данных в Firebase
void MainWindow::sendToFirebase(int lampNumber, int impact, float reactionTime)
{
    qDebug() << "SSL support available:" << QSslSocket::supportsSsl();
    qDebug() << "SSL library version:" << QSslSocket::sslLibraryVersionString();
    qDebug() << "SSL library build version:" << QSslSocket::sslLibraryBuildVersionString();

    QJsonObject json;
    json["lamp"] = lampNumber;
    json["impact"] = impact;
    json["reaction_time"] = reactionTime;

    QJsonDocument doc(json);
    QByteArray jsonData = doc.toJson();

    QNetworkRequest request(QUrl("https://boxtrain-65522-default-rtdb.firebaseio.com/data.json"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = networkManager->post(request, jsonData);
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Данные успешно отправлены в Firebase!";
        } else {
            qDebug() << "Ошибка отправки в Firebase:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

// Запуск тренировки
void MainWindow::on_start_training_Button_clicked()
{
    if (trainingSequence.isEmpty()) {
        qDebug() << "Попытка отправить пустую последовательность.";
        return;
    }

    QString dataString;
    for (int lamp : trainingSequence) {
        dataString += QString::number(lamp);
    }

    QByteArray data = dataString.toUtf8();
    QHostAddress receiverAddress(receiverIP);

    if (SocketUPD->writeDatagram(data, receiverAddress, receiverPort) == -1) {
        qDebug() << "Ошибка отправки данных:" << SocketUPD->errorString();
    } else {
        qDebug() << "Отправлена последовательность:" << dataString;
    }
}

// Запись последовательности ламп в тренировке
void MainWindow::on_create_training_Button_clicked()
{
    if (!recordingMode) {
        recordingMode = true;
        trainingSequence.clear();
        qDebug() << "Создание новой тренировки началось.";
    } else {
        recordingMode = false;
        qDebug() << "Создание тренировки завершено:" << trainingSequence;
    }
}

// Быстрая тренировка
void MainWindow::on_fast_training_Button_clicked()
{
    qDebug("Старт быстрой тренировки");
    startSending();
}

// Запись удара по лампе в тренировку
void MainWindow::on_lampButton_clicked(int lampNumber)
{
    if (recordingMode) {
        trainingSequence.append(lampNumber);
        qDebug() << "Записано нажатие лампы:" << lampNumber;

        QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(lampNumber));
        if (button) {
            button->setText(QString("Лампа %1\nВыбрана").arg(lampNumber));
        }
    }
}
