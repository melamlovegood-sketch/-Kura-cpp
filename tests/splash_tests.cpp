#include "SplashScreen.h"
#include "AppTheme.h"
#include <QDir>
#include <QPointer>
#include <QPushButton>
#include <QVariantAnimation>
#include <QtTest>

class SplashTests : public QObject {
    Q_OBJECT
private slots:
    void openingCoversWindowThenReturnsControl();
    void resizeFadeAndCloseDuringOpening();
    void previews();
};

void SplashTests::openingCoversWindowThenReturnsControl()
{
    QWidget window; window.resize(800,600);
    QPushButton button(QStringLiteral("Continue"), &window); button.setGeometry(10,10,120,40);
    window.show(); button.setFocus();
    QApplication::processEvents();
    QSignalSpy clicks(&button, &QPushButton::clicked);
    QPointer<SplashScreen> splash = SplashScreen::present(&window);
    QCOMPARE(SplashScreen::present(&window), splash.data());
    QCOMPARE(splash->geometry(), window.rect());
    QCOMPARE(window.childAt(button.geometry().center()), splash.data());
    QTest::mouseClick(splash, Qt::LeftButton, Qt::NoModifier, button.geometry().center());
    QCOMPARE(clicks.count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(splash.isNull(), 3500);
    QCOMPARE(window.childAt(button.geometry().center()), &button);
    QVERIFY(button.hasFocus());
    QTest::mouseClick(&button, Qt::LeftButton);
    QCOMPARE(clicks.count(), 1);
}

void SplashTests::resizeFadeAndCloseDuringOpening()
{
    QPointer<SplashScreen> splash;
    {
        QWidget window; window.resize(800,600);
        QPalette palette = window.palette(); palette.setColor(QPalette::Window, QColor("#204060"));
        window.setPalette(palette); window.setAutoFillBackground(true); window.show();
        splash = SplashScreen::present(&window);
        auto *timeline = splash->findChild<QVariantAnimation *>(QStringLiteral("splashTimeline"));
        QVERIFY(timeline); timeline->pause();
        QCOMPARE(window.grab().toImage().pixelColor(5,5), QColor("#F5F0E8"));
        window.resize(1180,860); QApplication::processEvents();
        QCOMPARE(splash->geometry(), window.rect());
        timeline->setCurrentTime(2650);
        const auto fading = window.grab().toImage().pixelColor(5,5);
        QVERIFY(fading.red() > 32 && fading.red() < 245);
        timeline->setCurrentTime(2799);
        const auto revealed = window.grab().toImage().pixelColor(5,5);
        QVERIFY(qAbs(revealed.red()-32) <= 1);
        QVERIFY(qAbs(revealed.blue()-96) <= 1);
        window.close();
    }
    QVERIFY(splash.isNull());
}

void SplashTests::previews()
{
    const QString output = qEnvironmentVariable("KURA_SPLASH_PREVIEW_DIR");
    if (output.isEmpty()) QSKIP("Optional visual review images.");
    QVERIFY(QDir().mkpath(output));
    for (const auto &theme : {"warm", "cool", "dark"}) {
        QWidget window; window.resize(1180,860); AppTheme::apply(&window, QLatin1String(theme));
        window.show();
        auto *splash = SplashScreen::present(&window);
        auto *timeline = splash->findChild<QVariantAnimation *>(); timeline->pause();
        for (int time : {0, 650, 1250, 2200}) {
            timeline->setCurrentTime(time); QApplication::processEvents();
            QVERIFY(window.grab().save(output + QStringLiteral("/%1-%2.png").arg(QLatin1String(theme)).arg(time)));
        }
        window.resize(800,600); timeline->setCurrentTime(2200); QApplication::processEvents();
        QVERIFY(window.grab().save(output + QStringLiteral("/%1-compact.png").arg(QLatin1String(theme))));
    }
}

QTEST_MAIN(SplashTests)
#include "splash_tests.moc"
