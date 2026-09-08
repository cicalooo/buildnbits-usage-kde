#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>

#include "ProviderRegistry.h"
#include "TrayIcon.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  KLocalizedString::setApplicationDomain("kdecodexbar");

  KAboutData aboutData("buildnbits-usage", i18n("BuildnBits Usage"), APP_VERSION,
                       i18n("Codex, Grok, and Antigravity remaining quota"), KAboutLicense::MIT,
                       i18n("(c) 2026 BuildnBits contributors"), QString(),
                       "https://github.com/cicalooo/buildnbits-usage-kde");
  KAboutData::setApplicationData(aboutData);

  app.setDesktopFileName("buildnbits-usage");
  app.setQuitOnLastWindowClosed(false);

  auto *registry = new ProviderRegistry(&app);
  new TrayIcon(registry, &app);

  return app.exec();
}
