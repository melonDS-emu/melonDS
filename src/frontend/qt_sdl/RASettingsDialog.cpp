#include "RASettingsDialog.h"
#include "ui_RASettingsDialog.h"

#include "Config.h"
#include "main.h"
#include "RetroAchievements/RAClient.h"

#include <QMessageBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

bool RASettingsDialog::needsReset = false;
RASettingsDialog::RASettingsDialog(EmuInstance* inst, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::RASettingsDialog)
    , emuInstance(inst)
{
    ui->setupUi(this);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RASettingsDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &RASettingsDialog::reject);

    setAttribute(Qt::WA_DeleteOnClose);

    auto& instcfg = emuInstance->getLocalConfig();
    ui->cbRAEnabled->setChecked(instcfg.GetBool("RetroAchievements.Enabled"));
    ui->cbRAHardcore->setChecked(instcfg.GetBool("RetroAchievements.HardcoreMode"));
    ui->leRAUsername->setText(QString::fromStdString(instcfg.GetString("RetroAchievements.Username")));
    ui->leRAPassword->setText(QString::fromStdString(instcfg.GetString("RetroAchievements.Password")));
    ui->cbRAAchievementToasts->setChecked(instcfg.GetBool("RetroAchievements.HideToasts"));
    ui->cbRAChallengeIndicators->setChecked(instcfg.GetBool("RetroAchievements.HideChallengeIndicators"));
    ui->cbRAEncoreMode->setChecked(instcfg.GetBool("RetroAchievements.EncoreMode"));
    ui->cbRAUnofficialMode->setChecked(instcfg.GetBool("RetroAchievements.Unofficial"));
    ui->cbRALeaderboardCounter->setChecked(instcfg.GetBool("RetroAchievements.HideLeaderboardCounter"));
    ui->cbRALeaderboardToasts->setChecked(instcfg.GetBool("RetroAchievements.HideLeaderboardToasts"));

    auto UpdateRAUI = [this]() {
        RAContext* ra = emuInstance->getRA();
        if (!ra) return;

        bool loggedIn = ra->IsLoggedIn();
        bool emuRunning = emuInstance->emuIsActive();

        ui->cbRAEnabled->setEnabled(!loggedIn);
        ui->cbRAHardcore->setEnabled(!loggedIn);
        ui->leRAUsername->setEnabled(!loggedIn);
        ui->leRAPassword->setEnabled(!loggedIn);
        ui->btnRALogin->setEnabled(!emuRunning);

        if (emuRunning) {
            ui->btnRALogin->setToolTip(
                "Stop the emulation before logging in or out of RetroAchievements."
            );
        } else {
            ui->btnRALogin->setToolTip(QString());
        }

        if (loggedIn) {
            ui->btnRALogin->setText("Logout");

            const rc_client_user_t* user = rc_client_get_user_info(ra->client);
            if (user && user->display_name)
                ui->lblRAStatus->setText(QString("Logged in as: %1").arg(user->display_name));
            else
                ui->lblRAStatus->setText("Logged in");

            ui->lblRAStatus->setStyleSheet("font-weight: bold; color: #2ecc71;");
        } else {
            ui->btnRALogin->setText("Login Now");
            ui->lblRAStatus->setText("Not logged in");
            ui->lblRAStatus->setStyleSheet("color: gray;");
        }
    };

    UpdateRAUI();

    connect(ui->btnRALogin, &QPushButton::clicked, this, [this, UpdateRAUI]() {
        RAContext* ra = emuInstance->getRA();
        if (!ra) return;

        if (ra->IsLoggedIn()) {
            ra->SetLoggedIn(false);
            ra->SetToken("");
            UpdateRAUI();

            ui->cbRAEnabled->setProperty("user_originalValue", !ui->cbRAEnabled->isChecked());
            ui->cbRAHardcore->setProperty("user_originalValue", !ui->cbRAHardcore->isChecked());
            ui->leRAUsername->setProperty("user_originalValue", "");
            ui->leRAPassword->setProperty("user_originalValue", "");

            if (emuInstance && emuInstance->emuIsActive()) {
                done(QDialog::Accepted);
            }

        } else {
            std::string user = ui->leRAUsername->text().toStdString();
            std::string pass = ui->leRAPassword->text().toStdString();
            if (user.empty() || pass.empty()) return;

            ra->LoginWithPassword(user.c_str(), pass.c_str(), ui->cbRAHardcore->isChecked());

            ui->cbRAEnabled->setProperty("user_originalValue", !ui->cbRAEnabled->isChecked());
            ui->cbRAHardcore->setProperty("user_originalValue", !ui->cbRAHardcore->isChecked());
            ui->leRAUsername->setProperty("user_originalValue", "");
            ui->leRAPassword->setProperty("user_originalValue", "");

            if (emuInstance && emuInstance->emuIsActive()) {
                done(QDialog::Accepted);
            }
        }
    });

    RAContext* raTmp = emuInstance->getRA();
    if (raTmp) {
        auto mainWin = emuInstance->getMainWindow();
        raTmp->onLoginResponse = [this, UpdateRAUI, mainWin](bool success, const std::string& msg) {
            QMetaObject::invokeMethod(this, [this, UpdateRAUI, mainWin, success, msg]() {
                UpdateRAUI();
                if (mainWin) {
                    mainWin->ShowRALoginToast(success, msg);
                }
            });
        };
    }

#define SET_ORIGVAL(type, val) \
    for (type* w : findChildren<type*>(nullptr)) \
        w->setProperty("user_originalValue", w->val());

    SET_ORIGVAL(QLineEdit, text);
    SET_ORIGVAL(QCheckBox, isChecked);

#undef SET_ORIGVAL
}

RASettingsDialog::~RASettingsDialog()
{
    if (emuInstance) {
        if (auto* ra = emuInstance->getRA()) {
            ra->onLoginResponse = nullptr;
        }
    }
    delete ui;
}

void RASettingsDialog::done(int r)
{
    if (!emuInstance)
    {
        QDialog::done(r);
        return;
    }

    needsReset = false;

#ifdef RETROACHIEVEMENTS_ENABLED
    if (r == QDialog::Accepted)
    {
        bool modified = false;

#define CHECK_ORIGVAL(type, val) \
        if (!modified) for (type* w : findChildren<type*>(nullptr)) \
        { \
            QVariant v = w->val(); \
            if (v != w->property("user_originalValue")) \
            { \
                modified = true; \
                break; \
            } \
        }

        CHECK_ORIGVAL(QLineEdit, text);
        CHECK_ORIGVAL(QCheckBox, isChecked);

#undef CHECK_ORIGVAL

        if (modified)
        {
            if (emuInstance->emuIsActive()
                && QMessageBox::warning(
                    this,
                    "Reset necessary to apply changes",
                    "The emulation will be reset for the changes to take place.",
                    QMessageBox::Ok,
                    QMessageBox::Cancel) != QMessageBox::Ok)
            {
                return;
            }

            auto& instcfg = emuInstance->getLocalConfig();

            instcfg.SetBool("RetroAchievements.Enabled", ui->cbRAEnabled->isChecked());
            instcfg.SetBool("RetroAchievements.HardcoreMode", ui->cbRAHardcore->isChecked());
            instcfg.SetString("RetroAchievements.Username", ui->leRAUsername->text().toStdString());
            instcfg.SetString("RetroAchievements.Password", ui->leRAPassword->text().toStdString());
            instcfg.SetBool("RetroAchievements.HideToasts", ui->cbRAAchievementToasts->isChecked());
            instcfg.SetBool("RetroAchievements.HideChallengeIndicators", ui->cbRAChallengeIndicators->isChecked());
            instcfg.SetBool("RetroAchievements.EncoreMode", ui->cbRAEncoreMode->isChecked());
            instcfg.SetBool("RetroAchievements.Unofficial", ui->cbRAUnofficialMode->isChecked());
            instcfg.SetBool("RetroAchievements.HideLeaderboardCounter", ui->cbRALeaderboardCounter->isChecked());
            instcfg.SetBool("RetroAchievements.HideLeaderboardToasts", ui->cbRALeaderboardToasts->isChecked());

            emuInstance->SyncRetroAchievementsFromConfig();
            Config::Save();

            needsReset = true;
        }
    }
#endif

    QDialog::done(r);
}