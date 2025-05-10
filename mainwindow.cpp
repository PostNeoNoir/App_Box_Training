#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QRandomGenerator>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QMessageBox>


#include "logindialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , receiverIP("192.168.4.1")
    , receiverPort(80)
{
    ui->setupUi(this);

    networkManager = new QNetworkAccessManager(this);
    pollTimer = new QTimer(this);

    logindialog logindialog(this);
    if (logindialog.exec() == QDialog::Accepted) {
        username = logindialog.getUsername();
        qDebug() << "Пользователь:" << username;

        QUrl url("http://" + receiverIP + "/login");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject obj;
        obj["username"] = username;
        QJsonDocument doc(obj);
        QByteArray data = doc.toJson();

        QNetworkReply *reply = networkManager->post(request, data);
        connect(reply, &QNetworkReply::finished, [reply]() {
            qDebug() << "Ответ от ESP (login):" << reply->readAll();
            reply->deleteLater();
        });
    }

    for (int i = 1; i <= 7; ++i) {
        QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(i));
        if (button) {
            connect(button, &QPushButton::clicked, this, [this, i]() {
                on_lampButton_clicked(i);
            });
        }
    }

    connect(pollTimer, &QTimer::timeout, this, &MainWindow::pollESP);

    resetButtonTexts();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::resetButtonTexts() {
    for (int i = 1; i <= 7; ++i) {
        QPushButton *btn = findChild<QPushButton *>(QString("pushButton_%1").arg(i));
        if (btn) {
            btn->setText(QString("Зона %1").arg(i));
        }
    }
}

void MainWindow::startSending() {
    accumulatedData.clear();
    for (int i = 0; i < 10; ++i) {
        int randomValue = QRandomGenerator::global()->bounded(1, 8);
        accumulatedData.append(QString::number(randomValue));
    }
    sendCommand();
}

void MainWindow::sendCommand() {
    QUrl url("http://" + receiverIP + "/sequence");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["username"] = username;
    obj["sequence"] = accumulatedData;
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();

    QNetworkReply *reply = networkManager->post(request, data);

    connect(reply, &QNetworkReply::finished, [reply]() {
        qDebug() << "Ответ от ESP:" << reply->readAll();
        reply->deleteLater();
    });

    pollTimer->start(1000);
}

void MainWindow::pollESP() {
    QUrl url("http://" + receiverIP + "/result?username=" + username);
    QNetworkRequest request(url);

    qDebug() << "URL для запроса:" << url.toString();

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        qDebug() << "Ответ от ESP:" << response;

        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (!doc.isArray()) {
            qDebug() << "Ошибка: Ожидаем массив данных";
            return;
        }

        QJsonArray trainings = doc.array();
        if (trainings.isEmpty()) {
            qDebug() << "Ошибка: Пустой массив данных";
            return;
        }

        QJsonArray innerArray = trainings.last().toArray();
        if (innerArray.isEmpty()) {
            qDebug() << "Ошибка: Внутренний массив пуст";
            return;
        }

        QJsonObject zoneData = innerArray.first().toObject();
        resetButtonTexts();

        for (int i = 1; i <= 7; ++i) {
            QString zoneKey = QString("zone%1").arg(i);
            QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(i));
            if (!button) continue;

            if (zoneData.contains(zoneKey)) {
                QJsonObject zone = zoneData[zoneKey].toObject();
                int impact = zone["impact"].toInt();
                double time = zone["time"].toDouble();

                button->setText(QString("Сила: %1\nВремя: %2с").arg(impact).arg(time, 0, 'f', 2));
                button->setStyleSheet("background-color: #45a049;");

                QTimer::singleShot(500, this, [button]() {
                    button->setStyleSheet("");  // Сброс цвета
                });

                qDebug() << "Зона" << i << "-> Сила:" << impact << "Время:" << time;
            }
        }
    });
}








void MainWindow::on_start_training_Button_clicked() {
    resetButtonTexts();
    if (trainingSequence.isEmpty()) {
        qDebug() << "Попытка отправить пустую последовательность. Создайте тренировку.";
        return;
    }

    accumulatedData.clear();
    for (int lamp : trainingSequence) {
        accumulatedData += QString::number(lamp);
    }
    sendCommand();
}

void MainWindow::on_create_training_Button_clicked() {
    if (!recordingMode) {
        recordingMode = true;
        trainingSequence.clear();
        resetButtonTexts();
        qDebug() << "Создание новой тренировки началось.";
    } else {
        recordingMode = false;
        qDebug() << "Создание тренировки завершено. Последовательность:" << trainingSequence;
    }
}

void MainWindow::on_fast_training_Button_clicked() {
    resetButtonTexts();
    qDebug("Быстрая тренировка запущена");
    startSending();
}

void MainWindow::on_lampButton_clicked(int lampNumber) {
    if (recordingMode) {
        trainingSequence.append(lampNumber);
        qDebug() << "Записано нажатие лампы:" << lampNumber;
        QPushButton *button = findChild<QPushButton *>(QString("pushButton_%1").arg(lampNumber));
        if (button) {
            button->setText(QString("Лампа %1\nВыбрана").arg(lampNumber));
        }
    }
}

void MainWindow::on_statistic_Button_clicked() {
    QUrl url("http://" + receiverIP + "/readfile?username=" + username);
    QNetworkRequest request(url);

    qDebug() << "URL для запроса:" << url.toString();

    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        qDebug() << "Ответ от ESP (сырой):" << response;

        // Проверяем, что ответ начинается с '[' (массив)
        if (response.isEmpty() || response.at(0) != '[') {
            qDebug() << "Ошибка: Ожидался JSON массив";
            return;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            qDebug() << "Ошибка парсинга JSON:" << parseError.errorString();
            return;
        }

        if (!doc.isArray()) {
            qDebug() << "Ошибка: Ожидался массив JSON";
            return;
        }

        QJsonArray trainingArray = doc.array();
        QVector<QJsonObject> userTrainings;

        // Фильтруем только тренировки текущего пользователя
        for (const QJsonValue &value : trainingArray) {
            if (!value.isObject()) continue;

            QJsonObject training = value.toObject();
            if (training["username"].toString() == username) {
                userTrainings.append(training);
            }
        }

        if (userTrainings.isEmpty()) {
            QMessageBox::information(this, "Статистика",
                                     "Нет данных о тренировках для пользователя " + username);
            return;
        }

        // Создаем окно статистики с графиками
        showStatisticWindowWithCharts(userTrainings);
    });
}


void MainWindow::onStatisticDataReceived(QNetworkReply *reply) {
    QByteArray responseData = reply->readAll();

    // Преобразуем байтовый ответ в строку, используя правильную кодировку UTF-8
    QString responseStr = QString::fromUtf8(responseData);

    // Преобразуем экранированные байты в символы
    responseStr = QByteArray::fromPercentEncoding(responseStr.toUtf8());

    // Преобразуем строку в JSON-документ
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseStr.toUtf8());

    if (jsonDoc.isNull()) {
        qDebug() << "Ошибка: Не удалось разобрать JSON!";
        return;
    }

    // Если это массив, продолжаем обработку
    if (jsonDoc.isArray()) {
        QJsonArray jsonArray = jsonDoc.array();

        for (const QJsonValue &value : jsonArray) {
            QJsonObject stats = value.toObject();
            qDebug() << stats["username"].toString();

            // Выводим или сохраняем данные статистики
            QJsonObject zones = stats["zones"].toObject();
            for (auto zoneKey : zones.keys()) {
                QJsonObject zone = zones[zoneKey].toObject();
                qDebug() << "Zone: " << zoneKey
                         << ", Impact: " << zone["impact"].toInt()
                         << ", Time: " << zone["time"].toDouble();
            }
        }
    } else {
        qDebug() << "Ошибка: Ожидался массив, но получен объект.";
    }
}







void MainWindow::showStatisticWindowWithCharts(const QVector<QJsonObject> &trainings)
{
    // Создаем окно
    QDialog *statsDialog = new QDialog(this);
    statsDialog->setWindowTitle("Статистика тренировок - " + username);
    statsDialog->resize(800, 600);

    // Создаем виджет для отрисовки графиков
    QWidget *chartsWidget = new QWidget(statsDialog);
    chartsWidget->setAttribute(Qt::WA_DeleteOnClose);

    // Переопределяем paintEvent для отрисовки графиков
    chartsWidget->installEventFilter(this);
    m_currentTrainings = trainings; // Сохраняем данные для отрисовки

    QVBoxLayout *layout = new QVBoxLayout(statsDialog);
    layout->addWidget(chartsWidget);

    statsDialog->exec();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Paint && !m_currentTrainings.isEmpty()) {
        QWidget *chartsWidget = qobject_cast<QWidget*>(watched);
        if (chartsWidget) {
            QPainter painter(chartsWidget);
            drawCharts(painter, chartsWidget->rect());
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::drawCharts(QPainter &painter, const QRect &rect)
{
    painter.fillRect(rect, Qt::white);
    painter.setRenderHint(QPainter::Antialiasing);

    // Рассчитываем статистику
    QVector<double> avgImpacts(7, 0), avgTimes(7, 0);
    QVector<int> counts(7, 0);

    for (const auto &training : m_currentTrainings) {
        QJsonObject zones = training["zones"].toObject();
        for (int i = 1; i <= 7; ++i) {
            QString zoneKey = QString("zone%1").arg(i);
            if (zones.contains(zoneKey)) {
                QJsonObject zone = zones[zoneKey].toObject();
                avgImpacts[i-1] += zone["impact"].toDouble();
                avgTimes[i-1] += zone["time"].toDouble();
                counts[i-1]++;
            }
        }
    }

    // Нормализуем данные
    for (int i = 0; i < 7; ++i) {
        if (counts[i] > 0) {
            avgImpacts[i] /= counts[i];
            avgTimes[i] /= counts[i];
        }
    }

    // Рисуем график силы удара (верхняя половина)
    QRect impactRect(rect.x(), rect.y(), rect.width(), rect.height()/2 - 10);
    drawBarChart(painter, impactRect, avgImpacts, "Средняя сила удара", QColor(65, 105, 225));

    // Рисуем график времени (нижняя половина)
    QRect timeRect(rect.x(), rect.y() + rect.height()/2 + 10,
                   rect.width(), rect.height()/2 - 10);
    drawBarChart(painter, timeRect, avgTimes, "Среднее время реакции (сек)", QColor(60, 179, 113));
}

void MainWindow::drawBarChart(QPainter &painter, const QRect &rect,
                              const QVector<double> &values,
                              const QString &title, const QColor &color)
{
    // Находим максимальное значение для масштабирования
    double maxValue = *std::max_element(values.begin(), values.end());
    if (maxValue == 0) maxValue = 1;

    // Рисуем заголовок
    painter.setPen(Qt::black);
    painter.drawText(rect, Qt::AlignTop | Qt::AlignHCenter, title);

    // Область для графика
    QRect chartRect = rect.adjusted(40, 30, -20, -30);

    // Рисуем оси
    painter.drawLine(chartRect.bottomLeft(), chartRect.bottomRight());
    painter.drawLine(chartRect.bottomLeft(), chartRect.topLeft());

    // Рисуем столбцы
    int barWidth = chartRect.width() / values.size();
    painter.setBrush(color);

    for (int i = 0; i < values.size(); ++i) {
        if (values[i] == 0) continue;

        int barHeight = (values[i] / maxValue) * chartRect.height();
        QRect barRect(
            chartRect.left() + i * barWidth + 5,
            chartRect.bottom() - barHeight,
            barWidth - 10,
            barHeight
            );

        painter.drawRect(barRect);

        // Подписи значений
        painter.drawText(barRect, Qt::AlignCenter, QString::number(values[i], 'f', 2));

        // Подписи зон
        painter.drawText(
            QRect(chartRect.left() + i * barWidth, chartRect.bottom() + 5, barWidth, 20),
            Qt::AlignHCenter | Qt::AlignTop,
            QString("Зона %1").arg(i+1)
            );
    }
}

