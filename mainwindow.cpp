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

    // Запрашиваем тип пользователя перед началом тренировки
    requestUserTypeAndStartTraining();
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

void MainWindow::requestUserTypeAndStartTraining() {
    // Запрашиваем ВСЕ данные через readfile
    QUrl url("http://" + receiverIP + "/readfile?username=" + username);
    QNetworkRequest request(url);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        qDebug() << "Получены данные тренировок:" << response;

        // Парсим JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);

        if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
            qDebug() << "Ошибка парсинга, используем тип по умолчанию";
            generateTrainingSequence("темповик");
            sendCommand();
            return;
        }

        // Ищем последнюю запись с типом для этого пользователя
        QString userType;
        QJsonArray trainings = doc.array();
        for (const QJsonValue &value : trainings) {
            if (value.isObject()) {
                QJsonObject training = value.toObject();
                if (training["username"].toString() == username &&
                    training.contains("type")) {
                    userType = training["type"].toString().toLower().trimmed();
                }
            }
        }

        // Если тип не найден, используем "темповик"
        if (userType.isEmpty() ||
            (userType != "нокаутер" && userType != "темповик" && userType != "игровик")) {
            qDebug() << "Тип не определен, используем по умолчанию";
            userType = "игровик";
        }

        qDebug() << "Определен тип пользователя:" << userType;
        generateTrainingSequence(userType);
        sendCommand();
    });
}

void MainWindow::generateTrainingSequence(const QString &userType) {
    trainingSequence.clear();
    accumulatedData.clear();

    // Определяем количество ударов в зависимости от типа
    int seriesLength = 10; // По умолчанию

    if (userType == "нокаутер") {
        seriesLength = 10; // Короткая тренировка
    }
    else if (userType == "игровик") {
        seriesLength = 15; // Средняя тренировка
    }
    else if (userType == "темповик") {
        seriesLength = 20; // Длинная тренировка
    }

    // Веса для зон в зависимости от типа пользователя
    QVector<int> zoneWeights = {1, 1, 1, 1, 1, 1, 1}; // По умолчанию все зоны равны

    if (userType == "нокаутер") {
        // Акцент на голову (3) и печень (7)
        zoneWeights = {1, 1, 3, 1, 1, 1, 3};
    }
    else if (userType == "темповик") {
        // Более равномерное распределение с акцентом на скорость
        zoneWeights = {2, 2, 1, 2, 2, 1, 1};
    }
    else if (userType == "игровик") {
        // Разнообразные удары с акцентом на корпус
        zoneWeights = {1, 2, 1, 2, 2, 1, 2};
    }

    // Генерируем последовательность
    int totalWeight = std::accumulate(zoneWeights.begin(), zoneWeights.end(), 0);

    for (int i = 0; i < seriesLength; ++i) {
        int randomValue = QRandomGenerator::global()->bounded(totalWeight);
        int accumulatedWeight = 0;
        int selectedZone = 1;

        for (int zone = 0; zone < zoneWeights.size(); ++zone) {
            accumulatedWeight += zoneWeights[zone];
            if (randomValue < accumulatedWeight) {
                selectedZone = zone + 1;
                break;
            }
        }

        trainingSequence.append(selectedZone);
        accumulatedData += QString::number(selectedZone);
    }

    qDebug() << "Сгенерирована" << seriesLength << "ударов для типа" << userType << ":" << trainingSequence;
}


void MainWindow::on_statistic_Button_clicked() {
    QUrl url("http://" + receiverIP + "/readfile?username=" + username);
    QNetworkRequest request(url);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);

        if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить статистику");
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
            QMessageBox::information(this, "Статистика", "Нет данных о тренировках");
            return;
        }

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

void MainWindow::showStatisticWindowWithCharts(const QVector<QJsonObject> &trainings) {
    // Анализируем текущую тренировку
    TrainingAnalysis analysis = analyzeTraining(trainings);

    // Создаем окно
    QDialog *statsDialog = new QDialog(this);
    statsDialog->setWindowTitle("Статистика тренировок - " + username);
    statsDialog->resize(800, 650);

    QVBoxLayout *mainLayout = new QVBoxLayout(statsDialog);

    // 1. Добавляем виджет с графиками
    QWidget *chartsWidget = new QWidget();
    chartsWidget->installEventFilter(this);
    m_currentTrainings = trainings;
    mainLayout->addWidget(chartsWidget, 3); // 3 части из 4

    // 2. Добавляем область с аналитикой
    QTextEdit *analysisText = new QTextEdit();
    analysisText->setReadOnly(true);
    analysisText->setStyleSheet("font-size: 14px;");

    QString analysisMessage;

    if (!trainings.isEmpty() && trainings.last().contains("type")) {
        QString userType = trainings.last()["type"].toString();
        if (userType == "нокаутер") {
            analysisMessage = "Тип: Нокаутер (короткая тренировка 10 ударов)\n\n";
        }
        else if (userType == "игровик") {
            analysisMessage = "Тип: Игровик (средняя тренировка 15 ударов)\n\n";
        }
        else if (userType == "темповик") {
            analysisMessage = "Тип: Темповик (длинная тренировка 20 ударов)\n\n";
        }
    }

    if (analysis.overallMessage.isEmpty()) {
        //analysisMessage = "Ваши показатели стабильны, продолжайте в том же духе!";
    } else {
        analysisMessage = analysis.overallMessage;
    }

    // Добавляем рекомендации по зонам
    for (int i = 0; i < analysis.zones.size(); ++i) {
        if (analysis.zones[i].needsAttention) {
            analysisMessage += "\n\n" + analysis.zones[i].message;
        }
    }

    analysisText->setText(analysisMessage);
    mainLayout->addWidget(analysisText, 1); // 1 часть из 4

    statsDialog->exec();
}

TrainingAnalysis MainWindow::analyzeTraining(const QVector<QJsonObject> &trainings) {
    TrainingAnalysis analysis;
    analysis.zones.resize(7); // Для 7 зон

    if (trainings.size() < 2) {
        analysis.overallMessage = "Недостаточно данных для анализа";
        return analysis;
    }

    // Берем последнюю тренировку
    QJsonObject lastTraining = trainings.last();
    QJsonObject lastZones = lastTraining["zones"].toObject();

    // Собираем данные по предыдущим тренировкам
    QVector<ZoneStats> previousStats(7);
    int validTrainings = 0;

    for (int i = 0; i < trainings.size()-1; ++i) {
        QJsonObject zones = trainings[i]["zones"].toObject();
        for (int zone = 1; zone <= 7; ++zone) {
            QString zoneKey = QString("zone%1").arg(zone);
            if (zones.contains(zoneKey)) {
                QJsonObject zoneData = zones[zoneKey].toObject();
                previousStats[zone-1].totalImpact += zoneData["impact"].toInt();
                previousStats[zone-1].totalTime += zoneData["time"].toDouble();
                previousStats[zone-1].count++;
            }
        }
        validTrainings++;
    }

    // Анализируем каждую зону
    for (int zone = 1; zone <= 7; ++zone) {
        QString zoneKey = QString("zone%1").arg(zone);

        if (lastZones.contains(zoneKey)) {
            QJsonObject currentZone = lastZones[zoneKey].toObject();
            int currentImpact = currentZone["impact"].toInt();
            double currentTime = currentZone["time"].toDouble();

            if (previousStats[zone-1].count > 0) {
                double avgPrevImpact = previousStats[zone-1].totalImpact / previousStats[zone-1].count;
                double avgPrevTime = previousStats[zone-1].totalTime / previousStats[zone-1].count;

                // Проверяем ухудшение показателей
                bool impactWorse = currentImpact < (avgPrevImpact * 0.9); // Ухудшение на 10%
                bool timeWorse = currentTime > (avgPrevTime * 1.1); // Ухудшение на 10%

                if (impactWorse || timeWorse) {
                    analysis.zones[zone-1].needsAttention = true;
                    QString message = QString("Обратите внимание на зону %1: ").arg(zone);

                    if (impactWorse && timeWorse) {
                        message += QString("удар стал слабее (было %1, сейчас %2) и медленнее (было %3с, сейчас %4с)")
                                       .arg(avgPrevImpact, 0, 'f', 0)
                                       .arg(currentImpact)
                                       .arg(avgPrevTime, 0, 'f', 2)
                                       .arg(currentTime, 0, 'f', 2);
                    }
                    else if (impactWorse) {
                        message += QString("удар стал слабее (было %1, сейчас %2)")
                                       .arg(avgPrevImpact, 0, 'f', 0)
                                       .arg(currentImpact);
                    }
                    else {
                        message += QString("удар стал медленнее (было %1с, сейчас %2с)")
                                       .arg(avgPrevTime, 0, 'f', 2)
                                       .arg(currentTime, 0, 'f', 2);
                    }

                    analysis.zones[zone-1].message = message;
                }
            }
        }
    }

    return analysis;
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

