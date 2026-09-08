#include "SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_settings(QStringLiteral("BuildnBits"), QStringLiteral("Usage"))
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(320);

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Refresh Interval:"), this));
    m_intervalCombo = new QComboBox(this);
    m_intervalCombo->addItem(tr("3 Minutes"), 180000);
    m_intervalCombo->addItem(tr("5 Minutes"), 300000);
    m_intervalCombo->addItem(tr("10 Minutes"), 600000);
    layout->addWidget(m_intervalCombo);

    layout->addSpacing(8);
    layout->addWidget(new QLabel(tr("Panel squares"), this));
    m_showCodex = new QCheckBox(tr("Codex"), this);
    m_showGrok = new QCheckBox(tr("Grok"), this);
    m_showAgy = new QCheckBox(tr("Antigravity"), this);
    layout->addWidget(m_showCodex);
    layout->addWidget(m_showGrok);
    layout->addWidget(m_showAgy);

    layout->addSpacing(8);
    m_autostartCheck = new QCheckBox(tr("Run at Startup"), this);
    layout->addWidget(m_autostartCheck);
    layout->addStretch();

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttonBox);

    loadSettings();
}

int SettingsDialog::refreshInterval() const {
    return m_intervalCombo->currentData().toInt();
}

bool SettingsDialog::isAutostartEnabled() const {
    return m_autostartCheck->isChecked();
}

bool SettingsDialog::showCodex() const { return m_showCodex->isChecked(); }
bool SettingsDialog::showGrok() const { return m_showGrok->isChecked(); }
bool SettingsDialog::showAgy() const { return m_showAgy->isChecked(); }

void SettingsDialog::loadSettings() {
    const int interval = m_settings.value(QStringLiteral("refresh_interval"), 300000).toInt();
    const int index = m_intervalCombo->findData(interval);
    m_intervalCombo->setCurrentIndex(index != -1 ? index : 1);
    m_autostartCheck->setChecked(m_settings.value(QStringLiteral("autostart"), false).toBool());
    m_showCodex->setChecked(m_settings.value(QStringLiteral("showCodex"), true).toBool());
    m_showGrok->setChecked(m_settings.value(QStringLiteral("showGrok"), true).toBool());
    m_showAgy->setChecked(m_settings.value(QStringLiteral("showAgy"), true).toBool());
}

void SettingsDialog::saveSettings() {
    m_settings.setValue(QStringLiteral("refresh_interval"), refreshInterval());
    m_settings.setValue(QStringLiteral("autostart"), isAutostartEnabled());
    m_settings.setValue(QStringLiteral("showCodex"), showCodex());
    m_settings.setValue(QStringLiteral("showGrok"), showGrok());
    m_settings.setValue(QStringLiteral("showAgy"), showAgy());
    updateAutostart(isAutostartEnabled());
    emit settingsChanged();
    accept();
}

void SettingsDialog::updateAutostart(bool enable) {
    const QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
    QDir dir(autostartDir);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));
    const QString desktopFile = autostartDir + QStringLiteral("/buildnbits-usage.desktop");
    if (enable) {
        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << QStringLiteral("[Desktop Entry]\n");
            out << QStringLiteral("Type=Application\n");
            out << QStringLiteral("Name=BuildnBits Usage\n");
            out << QStringLiteral("Exec=") << QCoreApplication::applicationFilePath() << QStringLiteral("\n");
            out << QStringLiteral("Icon=buildnbits-usage\n");
            out << QStringLiteral("Comment=AI Usage Tracker\n");
            out << QStringLiteral("X-GNOME-Autostart-enabled=true\n");
            out << QStringLiteral("Terminal=false\n");
        }
    } else {
        QFile::remove(desktopFile);
    }
}
