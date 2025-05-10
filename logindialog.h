#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class logindialog;
}

class logindialog : public QDialog
{
    Q_OBJECT

public:
    explicit logindialog(QWidget *parent = nullptr);
    ~logindialog();

    QString getUsername() const;

private slots:
    void on_pushButton_clicked();

private:
    Ui::logindialog *ui;
};

#endif // LOGINDIALOG_H
