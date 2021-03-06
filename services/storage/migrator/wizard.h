/*
 * <one line to give the library's name and an idea of what it does.>
 * Copyright 2013  Vishesh Handa <me@vhanda.in>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef NEPOMUK2_MIGRATIONWIZARD_H
#define NEPOMUK2_MIGRATIONWIZARD_H

#include "ui_errorpage.h"
#include "storageinterface.h"

#include <QtGui/QWizard>
#include <QtGui/QWizardPage>
#include <QRadioButton>
#include <QGroupBox>
#include <QProgressBar>

typedef org::kde::nepomuk::Storage StorageService;

namespace Nepomuk2 {

class MigrationWizard : public QWizard
{
    Q_OBJECT
public:
    explicit MigrationWizard(QWidget* parent = 0, Qt::WindowFlags flags = 0);

    enum PageIds {
        Id_MainPage = 0,
        Id_MigrationPage,
        Id_FinishPage,
        Id_ErrorPage
    };
    void showError(const QString& error);
};


class MainPage : public QWizardPage {
public:
    MainPage(QWidget* parent = 0);
    virtual int nextId() const;
private:
};

class MigrationPage : public QWizardPage {
    Q_OBJECT
public:
    MigrationPage(QWidget* parent = 0);
    virtual int nextId() const;

    virtual void initializePage();
    virtual bool isComplete() const;

private slots:
    void slotMigrationDone();
    void slotMigrationPercent(int percent);

private:
    bool m_done;
    StorageService* m_storageService;
    QProgressBar* m_progressBar;
};

class FinishPage : public QWizardPage {
public:
    FinishPage(QWidget* parent = 0);
    virtual int nextId() const;
};

class ErrorPage : public QWizardPage, public Ui::ErrorPage {
    Q_OBJECT
    Q_PROPERTY(QString errorMessage READ message WRITE setMessage)
public:
    ErrorPage(QWidget* parent = 0);

    QString message() const;
public slots:
    void setMessage(const QString& message);
};

}

#endif // NEPOMUK2_MIGRATIONWIZARD_H
