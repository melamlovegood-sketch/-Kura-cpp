#include "DemoMode.h"
#include "AppTheme.h"
#include "ShellWidgets.h"
#include "SplashScreen.h"
#include "mainwindow.h"
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>

DemoProfileDialog::DemoProfileDialog(QWidget *parent):QDialog(parent) {
    setObjectName("demoProfileDialog"); setWindowTitle(QStringLiteral("选择演示身份"));
    setMinimumWidth(640); resize(760,460); AppTheme::apply(this,"warm");
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(28,26,28,22); layout->setSpacing(18);
    auto *heading=ShellWidgets::label(QStringLiteral("选一段生活，开始体验"),this); heading->setProperty("billingTitle",true); layout->addWidget(heading);
    auto *intro=ShellWidgets::label(QStringLiteral("两套独立的虚构数据，已有近半年消费、清单、记价和复盘。"),this,"muted"); intro->setWordWrap(true); layout->addWidget(intro);
    auto *cards=new QHBoxLayout; cards->setSpacing(16);
    for (auto persona:{DemoPersonas::Persona::Student,DemoPersonas::Persona::Worker}) {
        auto *card=new QFrame(this); card->setProperty("billingCard",true);
        auto *column=new QVBoxLayout(card); column->setContentsMargins(20,20,20,20); column->setSpacing(14);
        auto *title=ShellWidgets::label(DemoPersonas::title(persona),card); title->setProperty("billingSubtitle",true); column->addWidget(title);
        auto *description=ShellWidgets::label(DemoPersonas::description(persona),card); description->setWordWrap(true); column->addWidget(description);
        column->addStretch();
        auto *button=new QPushButton(persona==DemoPersonas::Persona::Student?QStringLiteral("体验学生生活"):QStringLiteral("体验职场生活"),card);
        button->setObjectName(persona==DemoPersonas::Persona::Student?"demoStudentButton":"demoWorkerButton"); button->setProperty("primary",true); column->addWidget(button);
        connect(button,&QPushButton::clicked,this,[this,persona] {m_persona=persona; accept();}); cards->addWidget(card,1);
    }
    layout->addLayout(cards,1);
    auto *hint=ShellWidgets::label(QStringLiteral("可在窗口底部切换身份，本次导入和修改会保留；退出程序后，两套演示数据一同删除。"),this,"muted"); hint->setWordWrap(true); layout->addWidget(hint);
}

int DemoMode::run(QApplication &application,const QString &requestedProfile) {
    using Persona=DemoPersonas::Persona;
    Persona persona=Persona::Student;
    if (!requestedProfile.isEmpty()) persona=requestedProfile=="worker"?Persona::Worker:Persona::Student;
    else {
        DemoProfileDialog chooser;
        if (chooser.exec()!=QDialog::Accepted) return 0;
        persona=chooser.persona();
    }
    DemoPersonas::Session session;
    bool playSplash=true;
    for (;;) {
        QString error; const auto path=session.open(persona,error);
        if (path.isEmpty()) { QMessageBox::critical(nullptr,QStringLiteral("演示启动失败"),error); return 1; }
        bool switching=false; Persona next=persona;
        MainWindow window(path); window.setWindowTitle(QStringLiteral("Kura C++ · %1 · 演示模式").arg(DemoPersonas::title(persona)));
        auto *banner=new QLabel(QStringLiteral("%1 · 全部虚构 · 退出后删除").arg(DemoPersonas::title(persona)),window.statusBar());
        banner->setObjectName("demoBanner");
        auto *change=new QPushButton(QStringLiteral("切换演示身份"),window.statusBar()); change->setObjectName("switchDemoProfileButton");
        window.statusBar()->addPermanentWidget(banner); window.statusBar()->addPermanentWidget(change);
        if (persona==Persona::Student) window.statusBar()->showMessage(QStringLiteral("本月消费留空，供现场导入；历史复盘可切换到上月查看。"));
        QObject::connect(change,&QPushButton::clicked,&window,[&] {
            DemoProfileDialog chooser(&window);
            if (chooser.exec()!=QDialog::Accepted || chooser.persona()==persona) return;
            next=chooser.persona(); switching=true; window.close();
        });
        window.show(); if (playSplash) SplashScreen::present(&window); playSplash=false;
        const auto code=application.exec();
        if (!switching) return code;
        persona=next;
    }
}
