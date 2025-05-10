#include "logindialog.h"
#include "C:/Users/Artem/Documents/Change_Data/build/Desktop_Qt_5_15_2_MinGW_32_bit-Debug/ui_logindialog.h"
#include <QMessageBox>  // Добавляем для QMessageBox
#include <QPushButton>  // Для QPushButton

logindialog::logindialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::logindialog)
{
    ui->setupUi(this);
    setWindowTitle("Вход в аккаунт");

    ui->pushButton->setText("Войти");

    // Автоматическое соединение по имени слота (альтернативный вариант)
    // connect(ui->pushButton, &QPushButton::clicked,
    //         this, &logindialog::on_pushButton_clicked);
}

logindialog::~logindialog()
{
    delete ui;
}

// Слот, автоматически соединяемый по имени (on_<имя объекта>_<сигнал>)
void logindialog::on_pushButton_clicked()
{
    if (!ui->lineEdit->text().isEmpty()) {
        accept();
    } else {
        QMessageBox::warning(this,
                             "Ошибка",
                             "Введите имя пользователя");
    }
}

QString logindialog::getUsername() const
{
    return ui->lineEdit->text().trimmed();
}
