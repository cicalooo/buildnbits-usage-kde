#pragma once

#include <QDialog>
#include <QSettings>

class QComboBox;
class QCheckBox;
class QDialogButtonBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    int refreshInterval() const;
    bool isAutostartEnabled() const;
    bool showCodex() const;
    bool showGrok() const;
    bool showAgy() const;

signals:
    void settingsChanged();

private slots:
    void saveSettings();
    void loadSettings();
    void updateAutostart(bool enable);

private:
    QComboBox *m_intervalCombo;
    QCheckBox *m_autostartCheck;
    QCheckBox *m_showCodex;
    QCheckBox *m_showGrok;
    QCheckBox *m_showAgy;
    QDialogButtonBox *m_buttonBox;
    QSettings m_settings;
};
