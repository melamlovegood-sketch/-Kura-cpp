#pragma once
#include <QApplication>
#include <QDropEvent>
#include <QImage>
#include <QMimeData>
#include <QWidget>
#include <functional>

// Intercept image drops before editors can insert their file URLs as text.
// Scope each receiver to its own window so nested dialogs keep their own images.
class ScreenshotDropTarget final : public QObject {
public:
    ScreenshotDropTarget(QWidget *owner,std::function<bool()> available,
                         std::function<bool(const QString &)> file,
                         std::function<bool(const QImage &)> image)
        :QObject(owner),m_owner(owner),m_available(std::move(available)),
          m_file(std::move(file)),m_image(std::move(image))
    {
        owner->setAcceptDrops(true);
        qApp->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override
    {
        if (event->type()!=QEvent::DragEnter && event->type()!=QEvent::DragMove && event->type()!=QEvent::Drop) return false;
        auto *widget=qobject_cast<QWidget *>(object);
        if (!widget || (widget!=m_owner && !m_owner->isAncestorOf(widget)) || widget->window()!=m_owner->window()) return false;
        auto *drop=static_cast<QDropEvent *>(event);
        const auto *mime=drop->mimeData();
        const auto urls=mime->urls();
        bool localFiles=false;
        for (const auto &url:urls) localFiles=localFiles || url.isLocalFile();
        if (!localFiles && !mime->hasImage()) return false;
        drop->ignore();
        if (!m_owner->isEnabled() || !m_available() || !(drop->possibleActions() & Qt::CopyAction)) return true;
        if (localFiles && (urls.size()!=1 || !urls.first().isLocalFile())) return true;
        if (event->type()==QEvent::Drop) {
            const bool accepted=localFiles?m_file(urls.first().toLocalFile()):m_image(qvariant_cast<QImage>(mime->imageData()));
            if (!accepted) return true;
        }
        drop->setDropAction(Qt::CopyAction); drop->accept();
        return true;
    }
private:
    QWidget *m_owner;
    std::function<bool()> m_available;
    std::function<bool(const QString &)> m_file;
    std::function<bool(const QImage &)> m_image;
};
