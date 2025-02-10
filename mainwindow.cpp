#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QtEndian>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , receiverIP("192.168.1.100")  // IP
    , receiverPort(8888)           // ESP порт
{
    ui->setupUi(this);

    for (int i = 1; i <= 7; ++i) {
        QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(i));
        if (button) {
            connect(button, &QPushButton::clicked, this, [this, i]() {
                on_lampButton_clicked(i); // Передаём номер кнопки в общий слот
            });
        }
    }

    SocketUPD = new QUdpSocket(this); // Объект для UDP
    Port = 8888;
    SocketUPD->bind(Port);

    connect(SocketUPD, &QUdpSocket::readyRead, this, &MainWindow::onReadyRead);

    //connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::on_startButton_clicked);
    //Уже подключено по дефолту
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Функция генерации и отправки данных
void MainWindow::startSending()
{
    accumulatedData.clear();
    for (int i = 0; i < 10; ++i) {
        int randomValue = QRandomGenerator::global()->bounded(1,8); // 2-9
        accumulatedData.append(QString::number(randomValue));
    }

    sendCommand(); // Отправляем данные один раз
}

void MainWindow::sendCommand()
{
    QByteArray datagram = accumulatedData.toUtf8(); // Конвертируем строку в QByteArray
    QHostAddress receiverAddress(receiverIP);       // IP точки Wi-Fi (ESP)
    SocketUPD->writeDatagram(datagram, receiverAddress, receiverPort);
    qDebug() << "Отправил " << datagram;
}

void MainWindow::onReadyRead()
{
    while (SocketUPD->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(SocketUPD->pendingDatagramSize());
        SocketUPD->readDatagram(datagram.data(), datagram.size());

        if (datagram.size() == 7) { // Проверяем размер пакета
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

            // Обновляем текст кнопки
            QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(lampNumber));
            if (button) {
                button->setText(QString("Сила удара: %1\nВремя: %2 сек")
                                    .arg(impact)
                                    .arg(reactionTime, 0, 'f', 2));
            }
        } else {
            qDebug() << "Неверный размер пакета!";
        }
    }
}

void MainWindow::on_start_training_Button_clicked()
{
    if (trainingSequence.isEmpty()) {
        qDebug() << "Попытка отправить пустую последовательность. Создайте тренировку.";
        return;
    }

    // Формируем строку из последовательности
    QString dataString;
    for (int lamp : trainingSequence) {
        dataString += QString::number(lamp); // Добавляем число в строку без разделителей
    }

    // Конвертируем строку в байты для отправки
    QByteArray data = dataString.toUtf8();

    // Проверяем корректность IP-адреса
    QHostAddress receiverAddress(receiverIP);

    // Отправляем данные в Arduino
    if (SocketUPD->writeDatagram(data, receiverAddress, receiverPort) == -1) {
        qDebug() << "Ошибка при отправке данных:" << SocketUPD->errorString();
    } else {
        qDebug() << "Отправлена последовательность:" << dataString;
        qDebug() << "На адрес:" << receiverIP << ", порт:" << receiverPort;
    }

}


void MainWindow::on_create_training_Button_clicked()
{
    if (!recordingMode) {
        // Включаем режим записи
        recordingMode = true;
        trainingSequence.clear(); // Очищаем список последовательности
        //ui->statusLabel->setText("Режим записи: активен");
        qDebug() << "Создание новой тренировки началось.";
    } else {
        // Завершаем режим записи
        recordingMode = false;
        //ui->statusLabel->setText("Режим записи: завершён");
        qDebug() << "Создание тренировки завершено. Последовательность:" << trainingSequence;
    }

}


void MainWindow::on_fast_training_Button_clicked()
{
    qDebug("Start");
    startSending();
}

void MainWindow::on_lampButton_clicked(int lampNumber)
{
    if (recordingMode) {
        trainingSequence.append(lampNumber); // Добавляем номер лампы в последовательность
        qDebug() << "Записано нажатие лампы:" << lampNumber;

        // Обновляем текст кнопки для индикации
        QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(lampNumber));
        if (button) {
            button->setText(QString("Лампа %1\nВыбрана").arg(lampNumber));
        }
    }
}
