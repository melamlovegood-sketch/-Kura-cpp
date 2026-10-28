#include "AppTypography.h"
#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QResource>
#include <QWidget>

static void initializeTypographyResources() { Q_INIT_RESOURCE(kura); }

namespace {
bool usesKuraTypography(QWidget *widget)
{
    for (auto *scope = widget; scope; scope = scope->parentWidget())
        if (scope->property("kuraTypography").toBool()) return true;
    return false;
}

// Style sheets change fonts after construction. Keep the actual variable-font
// axes in sync, including dialogs, late-created rows and theme changes.
class TypographyFilter final : public QObject {
public:
    explicit TypographyFilter(QObject *parent) : QObject(parent) {}
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (!m_resolving && (event->type() == QEvent::FontChange || event->type() == QEvent::Show)) {
            auto *widget = qobject_cast<QWidget *>(object);
            if (widget && usesKuraTypography(widget)) {
                const auto font = AppTypography::resolved(widget->font());
                if (font != widget->font()) {
                    m_resolving = true;
                    widget->setFont(font);
                    m_resolving = false;
                }
            }
        }
        return false;
    }
private:
    bool m_resolving = false;
};
}

void AppTypography::initialize()
{
    static const bool loaded = [] {
        initializeTypographyResources();
        for (const auto &file : {"DMSans.ttf", "DMSans-Italic.ttf", "DMSerifDisplay-Regular.ttf", "DMSerifDisplay-Italic.ttf",
                                 "SourceHanSansCN-VF.otf", "LXGWWenKaiLite-Regular.ttf"})
            QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/") + QLatin1String(file));
#ifdef Q_OS_WIN
        // The offscreen Qt platform omits Windows TTC collections. Register the
        // installed files explicitly so visual tests use the same CJK faces as
        // the Windows browser and real application. These files are not bundled.
        const auto fonts = qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts/");
        for (const auto &file : {"msyh.ttc", "simsun.ttc"})
            if (QFile::exists(fonts + QLatin1String(file)))
                QFontDatabase::addApplicationFont(fonts + QLatin1String(file));
#endif
        if (qApp) qApp->installEventFilter(new TypographyFilter(qApp));
        return true;
    }();
    Q_UNUSED(loaded);
}

QFont AppTypography::resolved(QFont font)
{
    bool isSans = false, isSerif = false;
    for (const auto &family : font.families()) {
        isSans |= family == QStringLiteral("DM Sans");
        isSerif |= family == QStringLiteral("DM Serif Display");
    }
    if (!isSans && !isSerif) return font;
    font.setHintingPreference(QFont::PreferNoHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    font.setStyleName(QString());
    if (isSans) {
        const float pixels = font.pixelSize() > 0 ? font.pixelSize() : font.pointSizeF() * 96.f / 72.f;
        // DirectWrite can select the Thin named instance when only QFont weight
        // is supplied. Pin both axes instead of trusting the family-name match.
        font.setVariableAxis(QFont::Tag("wght"), float(font.weight()));
        font.setVariableAxis(QFont::Tag("opsz"), qBound(9.f, pixels, 40.f));
    } else {
        // The Latin display face is static, but its Chinese UI fallback is a
        // variable face whose default axis is ExtraLight. Pin its real weight.
        font.clearVariableAxes();
        font.setVariableAxis(QFont::Tag("wght"), float(font.weight()));
    }
    return font;
}

QFont AppTypography::sans(int pixels, QFont::Weight weight)
{
    initialize();
    QFont font;
    font.setFamilies({QStringLiteral("DM Sans"), QStringLiteral("Source Han Sans CN VF"), QStringLiteral("Microsoft YaHei"), QStringLiteral("PingFang SC")});
    font.setPixelSize(pixels); font.setWeight(weight);
    return resolved(font);
}

QFont AppTypography::serif(int pixels)
{
    initialize();
    QFont font;
    font.setFamilies({QStringLiteral("DM Serif Display"), QStringLiteral("Source Han Sans CN VF"), QStringLiteral("Microsoft YaHei")});
    font.setPixelSize(pixels); font.setWeight(QFont::Normal);
    return resolved(font);
}

QFont AppTypography::editorial(int pixels)
{
    initialize();
    QFont font;
    font.setFamilies({QStringLiteral("DM Serif Display"), QStringLiteral("LXGW WenKai Lite"), QStringLiteral("Source Han Sans CN VF")});
    font.setPixelSize(pixels); font.setWeight(QFont::Normal);
    return resolved(font);
}

void AppTypography::apply(QWidget *root)
{
    root->setProperty("kuraTypography", true);
    root->setFont(resolved(root->font()));
    for (auto *widget : root->findChildren<QWidget *>()) widget->setFont(resolved(widget->font()));
}
