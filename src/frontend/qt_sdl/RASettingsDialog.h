#pragma once

#include <QDialog>

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>

class EmuInstance;

namespace Ui {
class RASettingsDialog;
}

class RASettingsDialog : public QDialog
{
    Q_OBJECT

public:
    static bool needsReset;
    explicit RASettingsDialog(EmuInstance* inst, QWidget* parent = nullptr);
    ~RASettingsDialog() override;

    void done(int r) override;

private:
    Ui::RASettingsDialog* ui;

    QGroupBox* groupRA;
    QCheckBox* cbRAEnabled;
    QCheckBox* cbRAHardcore;
    QLineEdit* leRAUsername;
    QLineEdit* leRAPassword;
    QPushButton* btnRALogin;
    QLabel* lblRAStatus;

    EmuInstance* emuInstance;
};