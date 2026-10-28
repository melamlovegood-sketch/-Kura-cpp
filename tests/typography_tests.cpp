#include "AppTheme.h"
#include "AppTypography.h"
#include <QDialog>
#include <QFile>
#include <QGlyphRun>
#include <QLabel>
#include <QTextLayout>
#include <QtTest>
#include <cmath>

static QList<QGlyphRun> glyphs(const QString &text,const QFont &font)
{
    QTextLayout layout(text,font); layout.beginLayout(); auto line=layout.createLine();
    line.setLineWidth(2000); layout.endLayout(); return layout.glyphRuns();
}
static qreal advance(const QString &text,const QFont &font)
{
    QTextLayout layout(text,font); layout.beginLayout(); auto line=layout.createLine();
    line.setLineWidth(2000); layout.endLayout(); return line.naturalTextWidth();
}
static QSet<QString> families(const QString &text,const QFont &font)
{
    QSet<QString> result; for(const auto &run:glyphs(text,font)) result.insert(run.rawFont().familyName()); return result;
}

class TypographyTests:public QObject {
    Q_OBJECT
private slots:
    void originalFacesAndBrowserMetrics();
    void cjkUsesBundledFaces();
    void stylesAndLateDialogsKeepVariableAxes();
    void readableThemeContrast();
};

void TypographyTests::originalFacesAndBrowserMetrics()
{
    AppTypography::initialize();
    for(const auto &file:{"DMSans.ttf","DMSans-Italic.ttf","DMSerifDisplay-Regular.ttf","DMSerifDisplay-Italic.ttf", "SourceHanSansCN-VF.otf", "LXGWWenKaiLite-Regular.ttf"})
        QVERIFY(QFile::exists(QStringLiteral(":/fonts/")+QLatin1String(file)));
    for(auto weight:{QFont::Normal,QFont::Medium,QFont::DemiBold}) {
        const auto font=AppTypography::sans(15,weight);
        QCOMPARE(font.variableAxisValue(QFont::Tag("wght")),float(weight));
        QCOMPARE(font.variableAxisValue(QFont::Tag("opsz")),15.f);
        for(const auto &run:glyphs(QStringLiteral("Kura Settings 0123456789"),font)) {
            QCOMPARE(run.rawFont().familyName(),QStringLiteral("DM Sans"));
            QVERIFY(!run.rawFont().styleName().contains(QStringLiteral("Thin"),Qt::CaseInsensitive));
        }
    }
    // Browser CDP audit of the original project, at these exact CSS sizes.
    QVERIFY(qAbs(advance(QStringLiteral("Kura / AI Settings 0123456789"),AppTypography::sans(15))-208.46977)<0.6);
    QVERIFY(qAbs(advance(QStringLiteral("¥1,234.56 KURA"),AppTypography::serif(52))-359.68390)<0.6);
    auto italic=AppTypography::sans(15); italic.setItalic(true);
    for(const auto &run:glyphs(QStringLiteral("Kura"),italic))
        QVERIFY(run.rawFont().styleName().contains(QStringLiteral("Italic"),Qt::CaseInsensitive));
}

void TypographyTests::cjkUsesBundledFaces()
{
    QCOMPARE(families(QStringLiteral("把心动，留给真正值得的。"),AppTypography::sans(15)),QSet<QString>{QStringLiteral("Source Han Sans CN VF")});
    QCOMPARE(families(QStringLiteral("把钱花在真正喜欢的事上。"),AppTypography::editorial(24)),QSet<QString>{QStringLiteral("LXGW WenKai Lite")});
    const auto mixed=families(QStringLiteral("约 ¥299.00"),AppTypography::serif(16));
    QVERIFY(mixed.contains(QStringLiteral("Source Han Sans CN VF"))); QVERIFY(mixed.contains(QStringLiteral("DM Serif Display")));
    for (auto weight : {QFont::Normal,QFont::Medium,QFont::DemiBold}) {
        const auto normal=AppTypography::sans(15,weight);
        QCOMPARE(normal.variableAxisValue(QFont::Tag("wght")),float(weight));
        for (const auto &run:glyphs(QStringLiteral("账单清单执行复盘"),normal)) {
            QCOMPARE(run.rawFont().familyName(),QStringLiteral("Source Han Sans CN VF"));
            QVERIFY(run.rawFont().weight() >= QFont::Normal);
        }
    }
}

void TypographyTests::stylesAndLateDialogsKeepVariableAxes()
{
    QWidget root;
    auto *title=new QLabel(QStringLiteral("账单 Settings"),&root); title->setObjectName("billingPageTitle");
    auto *price=new QLabel(QStringLiteral("约 ¥299.00"),&root); price->setProperty("productAmount",true);
    auto *story=new QLabel(QStringLiteral("本月故事 Kura"),&root); story->setObjectName("monthlyStoryTitleLabel");
    for(const auto &theme:{"warm","cool","dark"}) {
        AppTheme::apply(&root,QLatin1String(theme)); root.show(); QApplication::processEvents();
        QCOMPARE(title->font().pixelSize(),20); QCOMPARE(title->font().weight(),QFont::Medium);
        QCOMPARE(title->font().variableAxisValue(QFont::Tag("wght")),500.f);
        QCOMPARE(title->font().variableAxisValue(QFont::Tag("opsz")),20.f);
        QCOMPARE(story->font().pixelSize(),24); QVERIFY(story->font().families().contains(QStringLiteral("LXGW WenKai Lite")));
        QCOMPARE(families(QStringLiteral("本月故事"),story->font()),QSet<QString>{QStringLiteral("LXGW WenKai Lite")});
        QCOMPARE(price->font().pixelSize(),16); QVERIFY(price->font().families().contains(QStringLiteral("DM Serif Display")));
        QDialog dialog(&root); auto *late=new QLabel(QStringLiteral("稍后创建 Settings"),&dialog); late->setProperty("heading",true);
        dialog.show(); QApplication::processEvents();
        QCOMPARE(late->font().weight(),QFont::Medium);
        QCOMPARE(late->font().variableAxisValue(QFont::Tag("wght")),500.f);
        for(const auto &run:glyphs(QStringLiteral("Settings"),late->font()))
            QVERIFY(!run.rawFont().styleName().contains(QStringLiteral("Thin"),Qt::CaseInsensitive));
    }
}

void TypographyTests::readableThemeContrast()
{
    const auto luminance=[](const QColor &color) {
        const auto linear=[](double c) { return c<=0.04045?c/12.92:std::pow((c+0.055)/1.055,2.4); };
        return .2126*linear(color.redF())+.7152*linear(color.greenF())+.0722*linear(color.blueF());
    };
    QWidget root;
    for (const auto &theme:{"warm","cool","dark"}) {
        root.setProperty("theme",QLatin1String(theme));
        for (const auto &surface:{"page","card","inset"}) {
            for (const auto &role:{"ink","secondary","muted"}) {
                const auto a=luminance(AppTheme::color(&root,QLatin1String(surface)));
                const auto b=luminance(AppTheme::color(&root,QLatin1String(role)));
                QVERIFY2((qMax(a,b)+.05)/(qMin(a,b)+.05)>=4.5,theme);
            }
        }
        const auto a=luminance(AppTheme::color(&root,"accent"));
        const auto b=luminance(AppTheme::color(&root,"onAccent"));
        QVERIFY2((qMax(a,b)+.05)/(qMin(a,b)+.05)>=4.5,theme);
    }
}

QTEST_MAIN(TypographyTests)
#include "typography_tests.moc"
