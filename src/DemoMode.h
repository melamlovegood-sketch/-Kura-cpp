#pragma once
#include "DemoPersonas.h"
#include <QDialog>
class QApplication;

class DemoProfileDialog : public QDialog {
public:
    explicit DemoProfileDialog(QWidget *parent=nullptr);
    DemoPersonas::Persona persona() const { return m_persona; }
private:
    DemoPersonas::Persona m_persona=DemoPersonas::Persona::Student;
};
namespace DemoMode {
int run(QApplication &application,const QString &requestedProfile={});
}
