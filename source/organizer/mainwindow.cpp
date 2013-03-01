/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <archive.h>
#include "spawn.h"
#include "report.h"
#include "modlist.h"
#include "profile.h"
#include "pluginlist.h"
#include "installdialog.h"
#include "profilesdialog.h"
#include "editexecutablesdialog.h"
#include "categories.h"
#include "categoriesdialog.h"
#include "utility.h"
#include "modinfodialog.h"
#include "overwriteinfodialog.h"
#include "activatemodsdialog.h"
#include "downloadlist.h"
#include "downloadlistwidget.h"
#include "downloadlistwidgetcompact.h"
#include "messagedialog.h"
#include "installationmanager.h"
#include "lockeddialog.h"
#include "syncoverwritedialog.h"
#include "logbuffer.h"
#include "downloadlistsortproxy.h"
#include "modlistsortproxy.h"
#include "motddialog.h"
#include "filedialogmemory.h"
#include "questionboxmemory.h"
#include "tutorialmanager.h"
#include "icondelegate.h"
#include "credentialsdialog.h"
#include "selectiondialog.h"
#include "csvbuilder.h"
#include "gameinfoimpl.h"
#include "savetextasdialog.h"
#include <gameinfo.h>
#include <appconfig.h>
#include <utility.h>
#include <map>
#include <ctime>
#include <QTime>
#include <util.h>
#include <QInputDialog>
#include <QSettings>
#include <QWhatsThis>
#include <wchar.h>
#include <sstream>
#include <QProcess>
#include <QMenu>
#include <QBuffer>
#include <QInputDialog>
#include <QDirIterator>
#include <QHelpEvent>
#include <QToolTip>
#include <QFileDialog>
#include <QTimer>
#include <QMessageBox>
#include <QDebug>
#include <QBuffer>
#include <QWidgetAction>
#include <QToolButton>
#include <QGraphicsObject>
#include <QPluginLoader>
#include <QRadioButton>
#include <QDesktopWidget>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <Psapi.h>
#include <shlobj.h>
#include <TlHelp32.h>


using namespace MOBase;
using namespace MOShared;


MainWindow::MainWindow(const QString &exeName, QSettings &initSettings, QWidget *parent)
  : QMainWindow(parent), ui(new Ui::MainWindow), m_Tutorial(this, "MainWindow"),
    m_ExeName(exeName), m_OldProfileIndex(-1),
    m_DirectoryStructure(new DirectoryEntry(L"data", NULL, 0)),
    m_ModList(NexusInterface::instance()),
    m_OldExecutableIndex(-1), m_GamePath(ToQString(GameInfo::instance().getGameDirectory())),
    m_NexusDialog(NexusInterface::instance()->getAccessManager(), NULL),
    m_DownloadManager(NexusInterface::instance(), this),
    m_InstallationManager(this), m_Translator(NULL), m_TranslatorQt(NULL),
    m_Updater(NexusInterface::instance(), this), m_CategoryFactory(CategoryFactory::instance()),
    m_CurrentProfile(NULL), m_AskForNexusPW(false), m_LoginAttempted(false),
    m_ArchivesInit(false), m_ContextItem(NULL), m_CurrentSaveView(NULL),
    m_GameInfo(new GameInfoImpl())
{
  ui->setupUi(this);

  this->setWindowTitle(ToQString(GameInfo::instance().getGameName()).append(" Mod Organizer v").append(m_Updater.getVersion().canonicalString()));

  m_RefreshProgress = new QProgressBar(statusBar());
  m_RefreshProgress->setTextVisible(true);
  m_RefreshProgress->setRange(0, 100);
  m_RefreshProgress->setValue(0);
  statusBar()->addWidget(m_RefreshProgress, 1000);
  statusBar()->clearMessage();

  ui->actionEndorseMO->setVisible(false);

  updateProblemsButton();

  updateToolBar();
//  ui->toolBar->blockSignals(true);

  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure);

  // set up mod list
  m_ModListSortProxy = new ModListSortProxy(m_CurrentProfile, this);
  m_ModListSortProxy->setSourceModel(&m_ModList);
  ui->modList->setModel(m_ModListSortProxy);

  ui->modList->sortByColumn(ModList::COL_PRIORITY, Qt::AscendingOrder);
  ui->modList->setItemDelegateForColumn(ModList::COL_FLAGS, new IconDelegate(m_ModListSortProxy));
  ui->modList->header()->installEventFilter(&m_ModList);
  ui->modList->header()->restoreState(initSettings.value("mod_list_state").toByteArray());
  ui->modList->installEventFilter(&m_ModList);
  // restoreState also seems to restores the resize mode from previous session,
  // I don't really like that
#if QT_VERSION >= 0x50000
  for (int i = 0; i < ui->modList->header()->count(); ++i) {
    ui->modList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
  }
  ui->modList->header()->setSectionResizeMode(ModList::COL_NAME, QHeaderView::Stretch);
#else
  for (int i = 0; i < ui->modList->header()->count(); ++i) {
    ui->modList->header()->setResizeMode(i, QHeaderView::ResizeToContents);
  }
  ui->modList->header()->setResizeMode(ModList::COL_NAME, QHeaderView::Stretch);
#endif

  // set up plugin list
  m_PluginListSortProxy = new PluginListSortProxy(this);
  m_PluginListSortProxy->setSourceModel(&m_PluginList);
  ui->espList->setModel(m_PluginListSortProxy);
  ui->espList->sortByColumn(PluginList::COL_PRIORITY, Qt::AscendingOrder);
  ui->espList->header()->restoreState(initSettings.value("plugin_list_state").toByteArray());
  ui->espList->installEventFilter(&m_PluginList);
#if QT_VERSION >= 0x50000
  for (int i = 0; i < ui->espList->header()->count(); ++i) {
    ui->espList->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
  }
  ui->espList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
#else
  for (int i = 0; i < ui->espList->header()->count(); ++i) {
    ui->espList->header()->setResizeMode(i, QHeaderView::ResizeToContents);
  }
  ui->espList->header()->setResizeMode(0, QHeaderView::Stretch);
#endif

  QMenu *linkMenu = new QMenu(this);
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Toolbar"), this, SLOT(linkToolbar()));
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Desktop"), this, SLOT(linkDesktop()));
  linkMenu->addAction(QIcon(":/MO/gui/link"), tr("Start Menu"), this, SLOT(linkMenu()));
  ui->linkButton->setMenu(linkMenu);

  m_DownloadManager.setOutputDirectory(m_Settings.getDownloadDirectory());

  NexusInterface::instance()->setCacheDirectory(m_Settings.getCacheDirectory());
  NexusInterface::instance()->setNMMVersion(m_Settings.getNMMVersion());

  updateDownloadListDelegate();
  connect(&m_ModList, SIGNAL(requestColumnSelect(QPoint)), this, SLOT(displayColumnSelection(QPoint)));

  connect(&m_DownloadManager, SIGNAL(showMessage(QString)), this, SLOT(showMessage(QString)));

  connect(NexusInterface::instance(), SIGNAL(requestNXMDownload(QString)), this, SLOT(downloadRequestedNXM(QString)));
  connect(&m_NexusDialog, SIGNAL(requestDownload(QNetworkReply*, int, QString)), this, SLOT(downloadRequested(QNetworkReply*, int, QString)));

  ui->savegameList->installEventFilter(this);
  ui->savegameList->setMouseTracking(true);
  connect(ui->savegameList, SIGNAL(itemEntered(QListWidgetItem*)),
          this, SLOT(saveSelectionChanged(QListWidgetItem*)));

  connect(&m_PluginList, SIGNAL(esplist_changed()), this, SLOT(esplist_changed()));
  connect(&m_ModList, SIGNAL(modorder_changed()), this, SLOT(modorder_changed()));
  connect(&m_ModList, SIGNAL(removeOrigin(QString)), this, SLOT(removeOrigin(QString)));
  connect(&m_ModList, SIGNAL(showMessage(QString)), this, SLOT(showMessage(QString)));
  connect(&m_ModList, SIGNAL(modRenamed(QString,QString)), this, SLOT(modRenamed(QString,QString)));
  connect(ui->modFilterEdit, SIGNAL(textChanged(QString)), m_ModListSortProxy, SLOT(updateFilter(QString)));
  connect(&m_ModList, SIGNAL(modlist_changed(int)), m_ModListSortProxy, SLOT(invalidate()));
  connect(&m_ModList, SIGNAL(removeSelectedMods()), this, SLOT(removeMod_clicked()));

  connect(&m_DirectoryRefresher, SIGNAL(refreshed()), this, SLOT(directory_refreshed()));
  connect(&m_DirectoryRefresher, SIGNAL(progress(int)), this, SLOT(refresher_progress(int)));
  connect(&m_DirectoryRefresher, SIGNAL(error(QString)), this, SLOT(showError(QString)));

  connect(&m_Settings, SIGNAL(languageChanged(QString)), this, SLOT(languageChange(QString)));
  connect(&m_Settings, SIGNAL(styleChanged(QString)), this, SIGNAL(styleChanged(QString)));

  connect(&m_Updater, SIGNAL(restart()), this, SLOT(close()));
  connect(&m_Updater, SIGNAL(updateAvailable()), this, SLOT(updateAvailable()));
  connect(&m_Updater, SIGNAL(motdAvailable(QString)), this, SLOT(motdReceived(QString)));

  connect(NexusInterface::instance()->getAccessManager(), SIGNAL(loginSuccessful(bool)), this, SLOT(loginSuccessful(bool)));
  connect(NexusInterface::instance()->getAccessManager(), SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)));

  connect(&TutorialManager::instance(), SIGNAL(windowTutorialFinished(QString)), this, SLOT(windowTutorialFinished(QString)));

  m_DirectoryRefresher.moveToThread(&m_RefresherThread);
  m_RefresherThread.start();

  setCompactDownloads(initSettings.value("compact_downloads", false).toBool());
  m_AskForNexusPW = initSettings.value("ask_for_nexuspw", true).toBool();
  setCategoryListVisible(initSettings.value("categorylist_visible", true).toBool());
  FileDialogMemory::restore(initSettings);

  fixCategories();
  m_Updater.testForUpdate();
  m_StartTime = QTime::currentTime();

  m_Tutorial.expose("modList", &m_ModList);
  m_Tutorial.expose("espList", &m_PluginList);
  loadPlugins();
}


MainWindow::~MainWindow()
{
  m_RefresherThread.exit();
  m_RefresherThread.wait();
  m_NexusDialog.close();
  delete ui;
  delete m_GameInfo;
}


void MainWindow::resizeEvent(QResizeEvent *event)
{
  m_Tutorial.resize(event->size());
  QMainWindow::resizeEvent(event);
}


void MainWindow::actionToToolButton(QAction *&sourceAction)
{
  QToolButton *button = new QToolButton(ui->toolBar);
  button->setObjectName(sourceAction->objectName());
  button->setIcon(sourceAction->icon());
  button->setText(sourceAction->text());
  button->setPopupMode(QToolButton::InstantPopup);
  button->setToolButtonStyle(ui->toolBar->toolButtonStyle());
  button->setToolTip(sourceAction->toolTip());
  button->setShortcut(sourceAction->shortcut());
  QMenu *buttonMenu = new QMenu(sourceAction->text());
  button->setMenu(buttonMenu);
  QAction *newAction = ui->toolBar->insertWidget(sourceAction, button);
  newAction->setObjectName(sourceAction->objectName());
  newAction->setIcon(sourceAction->icon());
  newAction->setText(sourceAction->text());
  newAction->setToolTip(sourceAction->toolTip());
  newAction->setShortcut(sourceAction->shortcut());
  ui->toolBar->removeAction(sourceAction);
  sourceAction->deleteLater();
  sourceAction = newAction;
}


void MainWindow::updateToolBar()
{
  foreach (QAction *action, ui->toolBar->actions()) {
    if (action->objectName().startsWith("custom__")) {
      ui->toolBar->removeAction(action);
    }
  }

  QWidget *spacer = new QWidget(ui->toolBar);
  spacer->setObjectName("custom__spacer");
  spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  QWidget *widget = ui->toolBar->widgetForAction(ui->actionTool);
  QToolButton *toolBtn = qobject_cast<QToolButton*>(widget);

  if (toolBtn->menu() == NULL) {
    actionToToolButton(ui->actionTool);
  }

  actionToToolButton(ui->actionHelp);
  createHelpWidget();

  foreach (QAction *action, ui->toolBar->actions()) {
    if (action->isSeparator()) {
      // insert spacers
      ui->toolBar->insertWidget(action, spacer);

      std::vector<Executable>::iterator begin, end;
      m_ExecutablesList.getExecutables(begin, end);
      for (auto iter = begin; iter != end; ++iter) {
        if (iter->m_Toolbar) {
          QAction *exeAction = new QAction(iconForExecutable(iter->m_BinaryInfo.filePath()),
                                           iter->m_Title,
                                           ui->toolBar);
          QVariant temp;
          temp.setValue(*iter);
          exeAction->setData(temp);
          exeAction->setObjectName(QString("custom__") + iter->m_Title);
          if (!connect(exeAction, SIGNAL(triggered()), this, SLOT(startExeAction()))) {
            qDebug("failed to connect trigger?");
          }
          ui->toolBar->insertAction(action, exeAction);
        }
      }
    }
  }
}


void MainWindow::updateProblemsButton()
{
  QString problemDescription;
  if (checkForProblems(problemDescription)) {
    ui->actionProblems->setEnabled(true);
    ui->actionProblems->setIconText(tr("Problems"));
    ui->actionProblems->setToolTip(tr("There are potential problems with your setup"));
  } else {
    ui->actionProblems->setIconText(tr("No Problems"));
    ui->actionProblems->setToolTip(tr("Everything seems to be in order"));
  }
}


bool MainWindow::errorReported(QString &logFile)
{
  QDir dir(ToQString(GameInfo::instance().getLogDir()));
  QFileInfoList files = dir.entryInfoList(QStringList("ModOrganizer_??_??_??_??_??.log"),
                                          QDir::Files, QDir::Name | QDir::Reversed);

  if (files.count() > 0) {
    logFile = files.at(0).absoluteFilePath();
    QFile file(logFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      char buffer[1024];
      int line = 0;
      while (!file.atEnd()) {
        file.readLine(buffer, 1024);
        if (strncmp(buffer, "ERROR", 5) == 0) {
          return true;
        }

        // prevent this function from taking forever
        if (line++ >= 50000) {
          break;
        }
      }
    }
  }

  return false;
}


bool MainWindow::checkForProblems(QString &problemDescription)
{
  problemDescription = "";

  foreach (IPluginDiagnose *diagnose, m_DiagnosisPlugins) {
    std::vector<unsigned int> activeProblems = diagnose->activeProblems();
    foreach (unsigned int key, activeProblems) {
      problemDescription.append(tr("<li>%1</li>").arg(diagnose->shortDescription(key)));
    }
  }

  QString NCCBinary = QCoreApplication::applicationDirPath().mid(0).append("/NCC/NexusClientCLI.exe");
  if (!QFile::exists(NCCBinary)) {
    problemDescription.append(tr("<li>NCC not installed. You won't be able to install some scripted mod-installers. Get NCC from <a href=\"http://skyrim.nexusmods.com/downloads/file.php?id=1334\">the MO page on nexus</a></li>"));
  } else {
    VS_FIXEDFILEINFO versionInfo = GetFileVersion(ToWString(QDir::toNativeSeparators(NCCBinary)));
    if ((versionInfo.dwFileVersionMS & 0xFFFF) != 0x02) {
      problemDescription.append(tr("<li>NCC version may be incompatible.</li>"));
    }
  }

  if (QSettings("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\NET Framework Setup\\NDP\\v3.5",
                QSettings::NativeFormat).value("Install", 0) != 1) {
    QString dotNetUrl = "http://www.microsoft.com/en-us/download/details.aspx?id=17851";
    problemDescription.append(tr("<li>dotNet is not installed or outdated. This is required to use NCC. "
                                 "Get it from here: <a href=\"%1\">%1</a></li>").arg(dotNetUrl));
  }

  QString logFile;
  if (errorReported(logFile)) {
    problemDescription.append(tr("<li>There was an error reported in your last log. Please see %1</li>").arg(logFile));
  }

  bool res = problemDescription.length() != 0;
  if (res) {
    problemDescription.prepend("<ul>").append("</ul>");

    ui->actionProblems->setEnabled(true);
    ui->actionProblems->setIconText(tr("Problems"));
    ui->actionProblems->setToolTip(tr("There are potential problems with your setup"));
  } else {
    ui->actionProblems->setToolTip(tr("Everything seems to be in order"));
  }
  return res;
}


void MainWindow::createHelpWidget()
{
  QToolButton *toolBtn = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionHelp));
  QMenu *buttonMenu = toolBtn->menu();

  QAction *helpAction = new QAction(tr("Help on UI"), buttonMenu);
  connect(helpAction, SIGNAL(triggered()), this, SLOT(helpTriggered()));
  buttonMenu->addAction(helpAction);

  QAction *wikiAction = new QAction(tr("Documentation Wiki"), buttonMenu);
  connect(wikiAction, SIGNAL(triggered()), this, SLOT(wikiTriggered()));
  buttonMenu->addAction(wikiAction);

  QAction *issueAction = new QAction(tr("Report Issue"), buttonMenu);
  connect(issueAction, SIGNAL(triggered()), this, SLOT(issueTriggered()));
  buttonMenu->addAction(issueAction);

  QMenu *tutorialMenu = new QMenu(tr("Tutorials"), buttonMenu);

  typedef std::vector<std::pair<int, QAction*> > ActionList;

  ActionList tutorials;

  QDirIterator dirIter(QApplication::applicationDirPath() + "/tutorials", QStringList("*.js"), QDir::Files);
  while (dirIter.hasNext()) {
    dirIter.next();
    QString fileName = dirIter.fileName();

    QFile file(dirIter.filePath());
    if (!file.open(QIODevice::ReadOnly)) {
      qCritical() << "Failed to open " << fileName;
      continue;
    }
    QString firstLine = QString::fromUtf8(file.readLine());
    if (firstLine.startsWith("//TL")) {
      QStringList params = firstLine.mid(4).trimmed().split('#');
      if (params.size() != 2) {
        qCritical() << "invalid header line for tutorial " << fileName << " expected 2 parameters";
        continue;
      }
      QAction *tutAction = new QAction(params.at(0), tutorialMenu);
      tutAction->setData(fileName);
      tutorials.push_back(std::make_pair(params.at(1).toInt(), tutAction));
    }
  }

  std::sort(tutorials.begin(), tutorials.end(),
            [](const ActionList::value_type &LHS, const ActionList::value_type &RHS) {
              return LHS.first < RHS.first; } );

  for (auto iter = tutorials.begin(); iter != tutorials.end(); ++iter) {
    connect(iter->second, SIGNAL(triggered()), this, SLOT(tutorialTriggered()));
    tutorialMenu->addAction(iter->second);
  }

  buttonMenu->addMenu(tutorialMenu);
}


bool MainWindow::saveCurrentLists()
{
  if (m_DirectoryUpdate) {
    qWarning("not saving lists during directory update");
    return false;
  }

  // save plugin list

  try {
    m_PluginList.saveTo(m_CurrentProfile->getPluginsFileName(),
                        m_CurrentProfile->getLoadOrderFileName(),
                        m_CurrentProfile->getLockedOrderFileName(),
                        m_CurrentProfile->getDeleterFileName(),
                        m_Settings.hideUncheckedPlugins());

    if (!m_PluginList.saveLoadOrder(*m_DirectoryStructure)) {
      MessageDialog::showMessage(tr("load order could not be saved"), this);
    } else {
      ui->btnSave->setEnabled(false);
    }
  } catch (const std::exception &e) {
    reportError(tr("failed to save load order: %1").arg(e.what()));
  }

  if (m_ArchivesInit) {
    QFile archiveFile(m_CurrentProfile->getArchivesFileName());
    if (archiveFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      for (int i = 0; i < ui->bsaList->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->bsaList->topLevelItem(i);
        if ((item != NULL) && (item->checkState(0) == Qt::Checked)) {
          archiveFile.write(item->text(0).toUtf8().append("\r\n"));
        }
      }
    } else {
      reportError(tr("failed to save archives order, do you have write access "
                     "to \"%1\"?").arg(m_CurrentProfile->getArchivesFileName()));
    }
    archiveFile.close();
  } else {
    qWarning("archive list not initialised");
  }
  return true;
}


bool MainWindow::addProfile()
{
  QComboBox *profileBox = findChild<QComboBox*>("profileBox");
  bool okClicked = false;

  QString name = QInputDialog::getText(this, tr("Name"),
                                       tr("Please enter a name for the new profile"),
                                       QLineEdit::Normal, QString(), &okClicked);
  if (okClicked && (name.size() > 0)) {
    try {
      profileBox->addItem(name);
      profileBox->setCurrentIndex(profileBox->count() - 1);
      return true;
    } catch (const std::exception& e) {
      reportError(tr("failed to create profile: %1").arg(e.what()));
      return false;
    }
  } else {
    return false;
  }
}


void MainWindow::hookUpWindowTutorials()
{
  QDirIterator dirIter(QApplication::applicationDirPath() + "/tutorials", QStringList("*.js"), QDir::Files);
  while (dirIter.hasNext()) {
    dirIter.next();
    QString fileName = dirIter.fileName();
    QFile file(dirIter.filePath());
    if (!file.open(QIODevice::ReadOnly)) {
      qCritical() << "Failed to open " << fileName;
      continue;
    }
    QString firstLine = QString::fromUtf8(file.readLine());
    if (firstLine.startsWith("//WIN")) {
      QString windowName = firstLine.mid(6).trimmed();
      if (!m_Settings.directInterface().value("CompletedWindowTutorials/" + windowName, false).toBool()) {
        TutorialManager::instance().activateTutorial(windowName, fileName);
      }
    }
  }
}


void MainWindow::showEvent(QShowEvent *event)
{
  refreshFilters();

  QMainWindow::showEvent(event);
  m_Tutorial.registerControl();

  hookUpWindowTutorials();

  if (m_Settings.directInterface().value("first_start", true).toBool()) {
    QString firstStepsTutorial = ToQString(AppConfig::firstStepsTutorial());
    if (TutorialManager::instance().hasTutorial(firstStepsTutorial)) {
      if (QMessageBox::question(this, tr("Show tutorial?"),
                                tr("You are starting Mod Organizer for the first time. "
                                   "Do you want to show a tutorial of its basic features? If you choose "
                                   "no you can always start the tutorial from the \"Help\"-menu."),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        TutorialManager::instance().activateTutorial("MainWindow", firstStepsTutorial);
      }
    } else {
      qCritical() << firstStepsTutorial << " missing";
      QPoint pos = ui->toolBar->mapToGlobal(QPoint());
      pos.rx() += ui->toolBar->width() / 2;
      pos.ry() += ui->toolBar->height();
      QWhatsThis::showText(pos,
          QObject::tr("Please use \"Help\" from the toolbar to get usage instructions to all elements"));
    }

    m_Settings.directInterface().setValue("first_start", false);
  }
}


void MainWindow::closeEvent(QCloseEvent* event)
{
  if (m_DownloadManager.downloadsInProgress() &&
    QMessageBox::question(this, tr("Downloads in progress"),
                          tr("There are still downloads in progress, do you really want to quit?"),
                          QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Cancel) {
    event->ignore();
    return;
  }

  setCursor(Qt::WaitCursor);

  m_NexusDialog.close();

  storeSettings();

  // profile has to be cleaned up before the modinfo-buffer is cleared
  delete m_CurrentProfile;
  m_CurrentProfile = NULL;

  ModInfo::clear();
  LogBuffer::cleanQuit();
  m_ModList.setProfile(NULL);
}


void MainWindow::createFirstProfile()
{
  if (!refreshProfiles(false)) {
    Profile newProf("Default", false);
    refreshProfiles(false);
  }
}


void MainWindow::setBrowserGeometry(const QByteArray &geometry)
{
  m_NexusDialog.restoreGeometry(geometry);
}


SaveGameGamebryo *MainWindow::getSaveGame(const QString &name)
{
  return new SaveGameGamebryo(this, name);
}


SaveGameGamebryo *MainWindow::getSaveGame(QListWidgetItem *item)
{
  try {
    SaveGameGamebryo *saveGame = getSaveGame(item->data(Qt::UserRole).toString());
    saveGame->setParent(item->listWidget());
    return saveGame;
  } catch (const std::exception &e) {
    reportError(tr("failed to read savegame: %1").arg(e.what()));
    return NULL;
  }
}


void MainWindow::displaySaveGameInfo(const SaveGameGamebryo *save, QPoint pos)
{
  if (m_CurrentSaveView == NULL) {
    m_CurrentSaveView = new SaveGameInfoWidgetGamebryo(save, &m_PluginList, this);
  } else {
    m_CurrentSaveView->setSave(save);
  }

  QRect screenRect = QApplication::desktop()->availableGeometry(m_CurrentSaveView);

  if (pos.x() + m_CurrentSaveView->width() > screenRect.right()) {
    pos.rx() -= (m_CurrentSaveView->width() + 2);
  } else {
    pos.rx() += 5;
  }

  if (pos.y() + m_CurrentSaveView->height() > screenRect.bottom()) {
    pos.ry() -= (m_CurrentSaveView->height() + 10);
  } else {
    pos.ry() += 20;
  }
  m_CurrentSaveView->move(pos);

  m_CurrentSaveView->show();
  ui->savegameList->activateWindow();
  connect(m_CurrentSaveView, SIGNAL(closeSaveInfo()), this, SLOT(hideSaveGameInfo()));
}


void MainWindow::saveSelectionChanged(QListWidgetItem *newItem)
{
  if (newItem == NULL) {
    hideSaveGameInfo();
  } else if ((m_CurrentSaveView == NULL) || (newItem != m_CurrentSaveView->property("displayItem").value<void*>())) {
    const SaveGameGamebryo *save = getSaveGame(newItem);
    if (save != NULL) {
      displaySaveGameInfo(save, QCursor::pos());
      m_CurrentSaveView->setProperty("displayItem", qVariantFromValue((void*)newItem));
    }
  }
}



void MainWindow::hideSaveGameInfo()
{
  if (m_CurrentSaveView != NULL) {
    disconnect(m_CurrentSaveView, SIGNAL(closeSaveInfo()), this, SLOT(hideSaveGameInfo()));
    m_CurrentSaveView->deleteLater();
    m_CurrentSaveView = NULL;
  }
}


bool MainWindow::eventFilter(QObject* object, QEvent *event)
{
  if ((object == ui->savegameList) &&
      ((event->type() == QEvent::Leave) || (event->type() == QEvent::WindowDeactivate))) {
    hideSaveGameInfo();
  }

  return false;
}


bool MainWindow::testForSteam()
{
  DWORD processIDs[1024];
  DWORD bytesReturned;
  if (!::EnumProcesses(processIDs, sizeof(processIDs), &bytesReturned)) {
    qWarning("failed to determine if steam is running");
    return true;
  }

  TCHAR processName[MAX_PATH];
  for (unsigned int i = 0; i < bytesReturned / sizeof(DWORD); ++i) {
    memset(processName, '\0', sizeof(TCHAR) * MAX_PATH);
    if (processIDs[i] != 0) {
      HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIDs[i]);

      if (process != NULL) {
        HMODULE module;
        DWORD ignore;

        // first module in a process is always the binary
        if (EnumProcessModules(process, &module, sizeof(HMODULE) * 1, &ignore)) {
          GetModuleBaseName(process, module, processName, MAX_PATH);
          if ((_tcsicmp(processName, TEXT("steam.exe")) == 0) ||
              (_tcsicmp(processName, TEXT("steamservice.exe")) == 0)) {
            return true;
          }
        }
      }
    }
  }

  return false;
}


bool MainWindow::verifyPlugin(IPlugin *plugin)
{
  if (plugin == NULL) {
    return false;
  } else if (!plugin->init(this)) {
    qWarning("plugin failed to initialize");
    return false;
  }
  return true;
}


void MainWindow::toolPluginInvoke()
{
  QAction *triggeredAction = qobject_cast<QAction*>(sender());
  IPluginTool *plugin = (IPluginTool*)triggeredAction->data().value<void*>();
  try {
    plugin->display();
  } catch (const std::exception &e) {
    reportError(tr("Plugin \"%1\" failed: %2").arg(plugin->name()).arg(e.what()));
  }
}


void MainWindow::registerPluginTool(IPluginTool *tool)
{
  QAction *action = new QAction(tool->icon(), tool->displayName(), ui->toolBar);
  action->setToolTip(tool->tooltip());
  tool->setParentWidget(this);
  action->setData(qVariantFromValue((void*)tool));
  connect(action, SIGNAL(triggered()), this, SLOT(toolPluginInvoke()));
  QToolButton *toolBtn = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionTool));
  toolBtn->menu()->addAction(action);
}


bool MainWindow::registerPlugin(QObject *plugin)
{
  { // generic treatment for all plugins
    IPlugin *pluginObj = qobject_cast<IPlugin*>(plugin);
    if (pluginObj == NULL) {
      return false;
    }
    m_Settings.registerPlugin(pluginObj);
  }

  { // diagnosis plugins
    IPluginDiagnose *diagnose = qobject_cast<IPluginDiagnose*>(plugin);
    if (diagnose != NULL) {
      m_DiagnosisPlugins.push_back(diagnose);
    }
  }

  { // tool plugins
    IPluginTool *tool = qobject_cast<IPluginTool*>(plugin);
    if (verifyPlugin(tool)) {
      registerPluginTool(tool);
      return true;
    }
  }
  { // installer plugins
    IPluginInstaller *installer = qobject_cast<IPluginInstaller*>(plugin);
    if (verifyPlugin(installer)) {
      installer->setParentWidget(this);
      m_InstallationManager.registerInstaller(installer);
      return true;
    }
  }
  return false;
}


void MainWindow::loadPlugins()
{
  m_DiagnosisPlugins.clear();

  m_Settings.clearPlugins();

  foreach (QObject *plugin, QPluginLoader::staticInstances()) {
    registerPlugin(plugin);
  }

  QString pluginPath = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOrganizerDirectory())) + "/" + ToQString(AppConfig::pluginPath());
  qDebug("looking for plugins in %s", pluginPath.toUtf8().constData());
  QDirIterator iter(pluginPath, QDir::Files | QDir::NoDotAndDotDot);
  while (iter.hasNext()) {
    iter.next();
    QString pluginName = iter.filePath();
    if (QLibrary::isLibrary(pluginName)) {
      QPluginLoader pluginLoader(pluginName);
      if (pluginLoader.instance() == NULL) {
        qCritical("failed to load plugin %s: %s",
                  pluginName.toUtf8().constData(), pluginLoader.errorString().toUtf8().constData());
      } else {
        if (registerPlugin(pluginLoader.instance())) {
          qDebug("loaded plugin \"%s\"", pluginName.toUtf8().constData());
        } else {
          qWarning("plugin \"%s\" failed to load", pluginName.toUtf8().constData());
        }
      }
    }
  }
}

IGameInfo &MainWindow::gameInfo() const
{
  return *m_GameInfo;
}


QString MainWindow::profileName() const
{
  return m_CurrentProfile->getName();
}

QString MainWindow::profilePath() const
{
  return m_CurrentProfile->getPath();
}

VersionInfo MainWindow::appVersion() const
{
  return m_Updater.getVersion();
}


IModInterface *MainWindow::getMod(const QString &name)
{
  unsigned int index = ModInfo::getIndex(name);
  if (index == UINT_MAX) {
    return NULL;
  } else {
    return ModInfo::getByIndex(index).data();
  }
}


IModInterface *MainWindow::createMod(const QString &name)
{
  unsigned int index = ModInfo::getIndex(name);
  if (index != UINT_MAX) {
    throw MyException(tr("The mod \"%1\" already exists!").arg(name));
  }

  QString targetDirectory = QDir::fromNativeSeparators(m_Settings.getModDirectory()).append("/").append(name.trimmed());

  QSettings settingsFile(targetDirectory.mid(0).append("/meta.ini"), QSettings::IniFormat);

  settingsFile.setValue("modid", 0);
  settingsFile.setValue("version", 0);
  settingsFile.setValue("newestVersion", 0);
  settingsFile.setValue("category", 0);
  settingsFile.setValue("installationFile", 0);
  return ModInfo::createFrom(QDir(targetDirectory), &m_DirectoryStructure).data();
}

bool MainWindow::removeMod(IModInterface *mod)
{
  return ModInfo::removeMod(ModInfo::getIndex(mod->name()));
}


void MainWindow::modDataChanged(IModInterface*)
{
  refreshModList();
}

QVariant MainWindow::pluginSetting(const QString &pluginName, const QString &key) const
{
  return m_Settings.pluginSetting(pluginName, key);
}


void MainWindow::startSteam()
{
  QSettings steamSettings("HKEY_CURRENT_USER\\Software\\Valve\\Steam",
                          QSettings::NativeFormat);
  QString exe = steamSettings.value("SteamExe", "").toString();
  if (!exe.isEmpty()) {
    QString temp = QString("\"%1\"").arg(exe);
    if (!QProcess::startDetached(temp)) {
      reportError(tr("Failed to start \"%1\"").arg(temp));
    } else {
      QMessageBox::information(this, tr("Waiting"), tr("Please press OK once you're logged into steam."));
    }
  }
}


HANDLE MainWindow::spawnBinaryDirect(const QFileInfo &binary, const QString &arguments, const QString &profileName,
                                    const QDir &currentDirectory, const QString &steamAppID)
{
  storeSettings();

  if (!binary.exists()) {
    reportError(tr("\"%1\" not found").arg(binary.fileName()));
    return INVALID_HANDLE_VALUE;
  }

  if (!steamAppID.isEmpty()) {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(steamAppID).c_str());
  } else {
    ::SetEnvironmentVariableW(L"SteamAPPId", ToWString(m_Settings.getSteamAppID()).c_str());
  }

  if ((GameInfo::instance().requiresSteam()) &&
      (m_Settings.getLoadMechanism() == LoadMechanism::LOAD_MODORGANIZER)) {
    if (!testForSteam()) {
      if (QuestionBoxMemory::query(this->isVisible() ? this : NULL,
            m_Settings.directInterface(), "steamQuery", tr("Start Steam?"),
            tr("Steam is required to be running already to correctly start the game. "
               "Should MO try to start steam now?"),
            QDialogButtonBox::Yes | QDialogButtonBox::No) == QDialogButtonBox::Yes) {
        startSteam();
      }
    }
  }

  return startBinary(binary, arguments, profileName, m_Settings.logLevel(), currentDirectory, true);
}


void MainWindow::spawnProgram(const QString &fileName, const QString &argumentsArg,
                              const QString &profileName, const QDir &currentDirectory)
{
  QFileInfo binary;
  QString arguments = argumentsArg;
  QString steamAppID;
  try {
    const Executable &exe = m_ExecutablesList.find(fileName);
    steamAppID = exe.m_SteamAppID;
    if (arguments == "") {
      arguments = exe.m_Arguments;
    }
    binary = exe.m_BinaryInfo;
  } catch (const std::runtime_error&) {
    qWarning("\"%s\" not set up as executable", fileName.toUtf8().constData());
    binary = QFileInfo(fileName);
  }
  spawnBinaryDirect(binary, arguments, profileName, currentDirectory, steamAppID);
}


void MainWindow::spawnBinary(const QFileInfo &binary, const QString &arguments, const QDir &currentDirectory, bool closeAfterStart, const QString &steamAppID)
{
  storeSettings();

  HANDLE processHandle = spawnBinaryDirect(binary, arguments, m_CurrentProfile->getName(), currentDirectory, steamAppID);
  if (processHandle != INVALID_HANDLE_VALUE) {
    if (closeAfterStart) {
      close();
    } else {
      this->setEnabled(false);

      LockedDialog *dialog = new LockedDialog(this);
      dialog->show();

      QCoreApplication::processEvents();

      while ((::WaitForSingleObject(processHandle, 1000) == WAIT_TIMEOUT) &&
              !dialog->unlockClicked()) {
        // keep processing events so the app doesn't appear dead
        QCoreApplication::processEvents();
      }

      this->setEnabled(true);
      refreshDirectoryStructure();
      refreshDataTree();
      if (GameInfo::instance().getLoadOrderMechanism() == GameInfo::TYPE_FILETIME) {
        QFile::remove(m_CurrentProfile->getLoadOrderFileName());
      }
      refreshLists();
      dialog->hide();
    }
  }
}


void MainWindow::startExeAction()
{
  QAction *action = qobject_cast<QAction*>(sender());
  if (action != NULL) {
    Executable selectedExecutable = action->data().value<Executable>();
    spawnBinary(selectedExecutable.m_BinaryInfo,
                selectedExecutable.m_Arguments,
                selectedExecutable.m_WorkingDirectory.length() != 0 ? selectedExecutable.m_WorkingDirectory
                                                                    : selectedExecutable.m_BinaryInfo.absolutePath(),
                selectedExecutable.m_CloseMO == DEFAULT_CLOSE,
                selectedExecutable.m_SteamAppID);
  } else {
    qCritical("not an action?");
  }
}


void MainWindow::refreshModList()
{
  ModInfo::updateFromDisc(m_Settings.getModDirectory(), &m_DirectoryStructure);
  m_CurrentProfile->refreshModStatus();

  m_ModList.notifyChange(-1);

  refreshDirectoryStructure();
}


void MainWindow::setExecutablesList(const ExecutablesList &executablesList)
{
  m_ExecutablesList = executablesList;
  refreshExecutablesList();
  updateToolBar();
}

void MainWindow::setExecutableIndex(int index)
{
  QComboBox *executableBox = findChild<QComboBox*>("executablesListBox");

  if ((index != 0) && (executableBox->count() > index)) {
    executableBox->setCurrentIndex(index);
  } else {
    executableBox->setCurrentIndex(1);
  }

  const Executable &selectedExecutable = executableBox->itemData(executableBox->currentIndex()).value<Executable>();

  QIcon addIcon(":/MO/gui/link");
  QIcon removeIcon(":/MO/gui/remove");

  QFileInfo linkDesktopFile(QDir::fromNativeSeparators(getDesktopDirectory()) + "/" + selectedExecutable.m_Title + ".lnk");
  QFileInfo linkMenuFile(QDir::fromNativeSeparators(getStartMenuDirectory()) + "/" + selectedExecutable.m_Title + ".lnk");

  ui->linkButton->menu()->actions().at(0)->setIcon(selectedExecutable.m_Toolbar ? removeIcon : addIcon);
  ui->linkButton->menu()->actions().at(1)->setIcon(linkDesktopFile.exists() ? removeIcon : addIcon);
  ui->linkButton->menu()->actions().at(2)->setIcon(linkMenuFile.exists() ? removeIcon : addIcon);
}


void MainWindow::activateSelectedProfile()
{
  QString profileName = ui->profileBox->currentText();
  qDebug() << "activate profile " << profileName;
  QString profileDir = QDir::fromNativeSeparators(ToQString(GameInfo::instance().getProfilesDir()))
                          .append("/").append(profileName);
  delete m_CurrentProfile;
  m_CurrentProfile = new Profile(QDir(profileDir));
  m_ModList.setProfile(m_CurrentProfile);

  m_ModListSortProxy->setProfile(m_CurrentProfile);

  connect(m_CurrentProfile, SIGNAL(modStatusChanged(uint)), this, SLOT(modStatusChanged(uint)));

  refreshModList();
  refreshSaveList();
}


void MainWindow::on_profileBox_currentIndexChanged(int index)
{
  if (ui->profileBox->isEnabled()) {
    int previousIndex = m_OldProfileIndex;
    m_OldProfileIndex = index;

    if ((previousIndex != -1) &&
        (m_CurrentProfile != NULL) &&
        m_CurrentProfile->exists()) {
      saveCurrentLists();
    }

    if (ui->profileBox->currentIndex() == 0) {
      ui->profileBox->setCurrentIndex(previousIndex);
      ProfilesDialog(m_GamePath).exec();
      while (!refreshProfiles()) {
        ProfilesDialog(m_GamePath).exec();
      }
    } else {
      activateSelectedProfile();
    }
  }
}


void MainWindow::updateTo(QTreeWidgetItem *subTree, const std::wstring &directorySoFar, const DirectoryEntry &directoryEntry, bool conflictsOnly)
{
  {
    std::vector<FileEntry*> files = directoryEntry.getFiles();
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      FileEntry *current = *iter;
      if (conflictsOnly && (current->getAlternatives().size() == 0)) {
        continue;
      }

      QString fileName = ToQString(current->getName());
      QStringList columns(fileName);
      bool isArchive = false;
      int originID = current->getOrigin(isArchive);
      QString source = ToQString(m_DirectoryStructure->getOriginByID(originID).getName());
      std::wstring archive = current->getArchive();
      if (archive.length() != 0) {
        source.append(" (").append(ToQString(archive)).append(")");
      }
      columns.append(source);
      QTreeWidgetItem *fileChild = new QTreeWidgetItem(columns);
      if (isArchive) {
        QFont font = fileChild->font(0);
        font.setItalic(true);
        fileChild->setFont(0, font);
        fileChild->setFont(1, font);
      } else if (fileName.endsWith(ModInfo::s_HiddenExt)) {
        QFont font = fileChild->font(0);
        font.setStrikeOut(true);
        fileChild->setFont(0, font);
        fileChild->setFont(1, font);
      }
      fileChild->setData(0, Qt::UserRole, ToQString(current->getFullPath()));
      fileChild->setData(0, Qt::UserRole + 1, isArchive);
      fileChild->setData(1, Qt::UserRole, source);
      fileChild->setData(1, Qt::UserRole + 1, originID);

      std::vector<int> alternatives = current->getAlternatives();

      if (!alternatives.empty()) {
        std::wostringstream altString;
        altString << ToWString(tr("Also in: <br>"));
        for (std::vector<int>::iterator altIter = alternatives.begin();
             altIter != alternatives.end(); ++altIter) {
          if (altIter != alternatives.begin()) {
            altString << " , ";
          }
          altString << "<span style=\"white-space: nowrap;\"><i>" << m_DirectoryStructure->getOriginByID(*altIter).getName() << "</font></span>";
        }
        fileChild->setToolTip(1, QString("%1").arg(ToQString(altString.str())));
        fileChild->setForeground(1, QBrush(Qt::red));
      } else {
        fileChild->setToolTip(1, tr("No conflict"));
      }
      subTree->addChild(fileChild);
    }
  }

  std::wostringstream temp;
  temp << directorySoFar << "\\" << directoryEntry.getName();
  {
    std::vector<DirectoryEntry*>::const_iterator current, end;
    directoryEntry.getSubDirectories(current, end);
    for (; current != end; ++current) {
      QStringList columns(ToQString((*current)->getName()));
      columns.append("");
      QTreeWidgetItem *directoryChild = new QTreeWidgetItem(columns);
      updateTo(directoryChild, temp.str(), **current, conflictsOnly);
      if (directoryChild->childCount() != 0) {
        subTree->addChild(directoryChild);
      }
    }
  }
  subTree->sortChildren(0, Qt::AscendingOrder);
}


bool MainWindow::refreshProfiles(bool selectProfile)
{
  QComboBox* profileBox = findChild<QComboBox*>("profileBox");

  QString currentProfileName = profileBox->currentText();

  profileBox->blockSignals(true);
  profileBox->clear();
  profileBox->addItem(QObject::tr("<Manage...>"));

  QDir profilesDir(ToQString(GameInfo::instance().getProfilesDir()));
  profilesDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);

  QDirIterator profileIter(profilesDir);

  int newIndex = profileIter.hasNext() ? 1 : 0;
  int currentIndex = 0;
  while (profileIter.hasNext()) {
    profileIter.next();
    ++currentIndex;
    try {
      profileBox->addItem(profileIter.fileName());
      if (currentProfileName == profileIter.fileName()) {
        newIndex = currentIndex;
      }
    } catch (const std::runtime_error& error) {
      reportError(QObject::tr("failed to parse profile %1: %2").arg(profileIter.fileName()).arg(error.what()));
    }
  }

  // now select one of the profiles, preferably the one that was selected before
  profileBox->blockSignals(false);

  if (selectProfile) {
    if (profileBox->count() > 1) {
      if (currentProfileName.length() != 0) {
        if ((newIndex != 0) && (profileBox->count() > newIndex)) {
          profileBox->setCurrentIndex(newIndex);
        } else {
          profileBox->setCurrentIndex(1);
        }
      }
      return true;
    } else {
      return false;
    }
  } else {
    return profileBox->count() > 1;
  }
}


void MainWindow::refreshDirectoryStructure()
{
  m_DirectoryUpdate = true;
  std::vector<std::tuple<QString, QString, int> > activeModList = m_CurrentProfile->getActiveMods();
  m_DirectoryRefresher.setMods(activeModList);

  statusBar()->show();
//  m_RefreshProgress->setVisible(true);
  m_RefreshProgress->setRange(0, 100);

  QTimer::singleShot(0, &m_DirectoryRefresher, SLOT(refresh()));
}


QIcon MainWindow::iconForExecutable(const QString &filePath)
{
  HICON winIcon;
  UINT res = ::ExtractIconExW(ToWString(filePath).c_str(), 0, &winIcon, NULL, 1);
  if (res == 1) {
    QIcon result = QIcon(QPixmap::fromWinHICON(winIcon));
    ::DestroyIcon(winIcon);
    return result;
  } else {
    return QIcon(":/MO/gui/executable");
  }
}


void MainWindow::refreshExecutablesList()
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");
  executablesList->setEnabled(false);
  executablesList->clear();
  executablesList->addItem(tr("<Edit...>"));

  QAbstractItemModel *model = executablesList->model();

  std::vector<Executable>::const_iterator current, end;
  m_ExecutablesList.getExecutables(current, end);
  for(int i = 0; current != end; ++current, ++i) {
    QVariant temp;
    temp.setValue(*current);
    QIcon icon = iconForExecutable(current->m_BinaryInfo.filePath());
    executablesList->addItem(icon, current->m_Title, temp);
    model->setData(model->index(i, 0), QSize(0, executablesList->iconSize().height() + 4), Qt::SizeHintRole);
  }

  setExecutableIndex(1);
  executablesList->setEnabled(true);
}


void MainWindow::refreshDataTree()
{
  QCheckBox *conflictsBox = findChild<QCheckBox*>("conflictsCheckBox");
  QTreeWidget *tree = findChild<QTreeWidget*>("dataTree");
  tree->clear();
  QStringList columns("data");
  columns.append("");
  QTreeWidgetItem *subTree = new QTreeWidgetItem(columns);
  updateTo(subTree, L"", *m_DirectoryStructure, conflictsBox->isChecked());
  tree->insertTopLevelItem(0, subTree);
  subTree->setExpanded(true);
  tree->header()->resizeSection(0, 200);
}


void MainWindow::refreshSaveList()
{
  refreshLists();
  ui->savegameList->clear();

  QDir savesDir;
  if (m_CurrentProfile->localSavesEnabled()) {
    savesDir.setPath(m_CurrentProfile->getPath() + "/saves");
  } else {
    wchar_t path[MAX_PATH];
    ::GetPrivateProfileStringW(L"General", L"SLocalSavePath", L"Saves",
                               path, MAX_PATH,
                               (ToWString(m_CurrentProfile->getPath()) + L"\\" + GameInfo::instance().getIniFileNames().at(0)).c_str());
    savesDir.setPath(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getDocumentsDir() + L"\\" + path)));
  }

  QStringList filters;
  filters << ToQString(GameInfo::instance().getSaveGameExtension());
  savesDir.setNameFilters(filters);

  QFileInfoList files = savesDir.entryInfoList(QDir::Files, QDir::Time);

  foreach (const QFileInfo &file, files) {
    QListWidgetItem *item = new QListWidgetItem(file.fileName());
    item->setData(Qt::UserRole, file.absoluteFilePath());
    ui->savegameList->addItem(item);
  }
}


void MainWindow::refreshLists()
{
  if (m_DirectoryStructure->isPopulated()) {
    refreshESPList();
    refreshBSAList();
  } // no point in refreshing lists if no files have been added to the directory tree
}


void MainWindow::refreshESPList()
{
  m_CurrentProfile->writeModlist();

  // clear list
  m_PluginList.refresh(m_CurrentProfile->getName(), *m_DirectoryStructure,
                       m_CurrentProfile->getPluginsFileName(),
                       m_CurrentProfile->getLoadOrderFileName(),
                       m_CurrentProfile->getLockedOrderFileName());
  //m_PluginList.readFrom(m_CurrentProfile->getPluginsFileName());

  findChild<QPushButton*>("btnSave")->setEnabled(false);
}


static bool BySortValue(const std::pair<UINT32, QTreeWidgetItem*> &LHS, const std::pair<UINT32, QTreeWidgetItem*> &RHS)
{
  return LHS.first < RHS.first;
}


void MainWindow::refreshBSAList()
{
  m_ArchivesInit = false;
  ui->bsaList->clear();
#if QT_VERSION >= 0x50000
  ui->bsaList->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
  ui->bsaList->header()->setResizeMode(QHeaderView::ResizeToContents);
#endif

  m_DefaultArchives.clear();

  wchar_t buffer[256];
  std::wstring iniFileName = ToWString(QDir::toNativeSeparators(m_CurrentProfile->getIniFileName()));
  if (::GetPrivateProfileStringW(L"Archive", GameInfo::instance().archiveListKey().c_str(),
                                 L"", buffer, 256, iniFileName.c_str()) != 0) {
    m_DefaultArchives = ToQString(buffer).split(',');
  }

  if (::GetPrivateProfileStringW(L"Archive", GameInfo::instance().archiveListKey().append(L"2").c_str(),
                                 L"", buffer, 256, iniFileName.c_str()) != 0) {
    m_DefaultArchives.append(ToQString(buffer).split(','));
  }

  for (int i = 0; i < m_DefaultArchives.count(); ++i) {
    m_DefaultArchives[i] = m_DefaultArchives[i].trimmed();
  }

  m_ActiveArchives.clear();

  QFile archiveFile(m_CurrentProfile->getArchivesFileName());
  if (archiveFile.open(QIODevice::ReadOnly)) {
    while (!archiveFile.atEnd()) {
      m_ActiveArchives.append(QString::fromUtf8(archiveFile.readLine()));
    }
    archiveFile.close();

    for (int i = 0; i < m_ActiveArchives.count(); ++i) {
      m_ActiveArchives[i] = m_ActiveArchives[i].trimmed();
    }
  } else {
    m_ActiveArchives = m_DefaultArchives;
  }

  std::vector<std::pair<UINT32, QTreeWidgetItem*> > items;

  std::vector<FileEntry*> files = m_DirectoryStructure->getFiles();
  for (auto iter = files.begin(); iter != files.end(); ++iter) {
    FileEntry *current = *iter;

    QString filename = ToQString(current->getName().c_str());
    QString extension = filename.right(3).toLower();

    if (extension == "bsa") {
      if (filename.compare("update.bsa", Qt::CaseInsensitive) == 0) {
        // hack: ignore update.bsa because it confuses people
        continue;
      }
      int index = m_ActiveArchives.indexOf(filename);
      QStringList strings(filename);
      bool isArchive = false;
      int origin = current->getOrigin(isArchive);
      strings.append(ToQString(m_DirectoryStructure->getOriginByID(origin).getName()));
      QTreeWidgetItem *newItem = new QTreeWidgetItem(strings);
      newItem->setData(0, Qt::UserRole, index);
      newItem->setData(1, Qt::UserRole, origin);
      newItem->setFlags(newItem->flags() & ~Qt::ItemIsDropEnabled | Qt::ItemIsUserCheckable);
      newItem->setCheckState(0, (index != -1) ? Qt::Checked : Qt::Unchecked);
      if (m_Settings.forceEnableCoreFiles() && m_DefaultArchives.contains(filename)) {
        newItem->setCheckState(0, Qt::Checked);
        newItem->setDisabled(true);
      } else {
        newItem->setCheckState(0, (index != -1) ? Qt::Checked : Qt::Unchecked);
      }

      if (index < 0) index = 0;
      UINT32 sortValue = ((m_DirectoryStructure->getOriginByID(origin).getPriority() & 0xFFFF) << 16) | (index & 0xFFFF);
      items.push_back(std::make_pair(sortValue, newItem));
    }
  }

  std::sort(items.begin(), items.end(), BySortValue);

  for (std::vector<std::pair<UINT32, QTreeWidgetItem*> >::iterator iter = items.begin(); iter != items.end(); ++iter) {
    ui->bsaList->addTopLevelItem(iter->second);
  }

  checkBSAList();
  m_ArchivesInit = true;
}


void MainWindow::checkBSAList()
{
  ui->bsaList->blockSignals(true);

  bool warning = false;

  for (int i = 0; i < ui->bsaList->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = ui->bsaList->topLevelItem(i);
    QString filename = item->text(0);
    item->setIcon(0, QIcon());
    item->setToolTip(0, QString());

    if (item->checkState(0) == Qt::Unchecked) {
      if (m_DefaultArchives.contains(filename)) {
        item->setIcon(0, QIcon(":/MO/gui/resources/dialog-warning.png"));
        item->setToolTip(0, tr("This bsa is enabled in the ini file so it may be required!"));
        warning = true;
      } else {
        QString espName = filename.mid(0, filename.length() - 3).append("esp").toLower();
        QString esmName = filename.mid(0, filename.length() - 3).append("esm").toLower();
        if (m_PluginList.isEnabled(espName) || m_PluginList.isEnabled(esmName)) {
          item->setIcon(0, QIcon(":/MO/gui/resources/dialog-warning.png"));
          item->setToolTip(0, tr("This archive will still be loaded since there is a plugin of the same name but "
                                    "its files will not follow installation order!"));
          warning = true;
        }
      }
    }
  }

  if (warning) {
    ui->tabWidget->setTabIcon(1, QIcon(":/MO/gui/resources/dialog-warning.png"));
  } else {
    ui->tabWidget->setTabIcon(1, QIcon());
  }

  ui->bsaList->blockSignals(false);
}


void MainWindow::fixCategories()
{
  for (unsigned int i = 0; i < ModInfo::getNumMods(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
    std::set<int> categories = modInfo->getCategories();
    for (std::set<int>::iterator iter = categories.begin();
         iter != categories.end(); ++iter) {
      if (!m_CategoryFactory.categoryExists(*iter)) {
        modInfo->setCategory(*iter, false);
      }
    }
  }
}


void MainWindow::readSettings()
{
  QSettings settings(ToQString(GameInfo::instance().getIniFilename()), QSettings::IniFormat);

  if (settings.contains("window_geometry")) {
    restoreGeometry(settings.value("window_geometry").toByteArray());
  }

  if (settings.contains("browser_geometry")) {
    setBrowserGeometry(settings.value("browser_geometry").toByteArray());
  }

  bool filtersVisible = settings.value("filters_visible", false).toBool();
  setCategoryListVisible(filtersVisible);
  ui->displayCategoriesBtn->setChecked(filtersVisible);

  languageChange(m_Settings.language());
  int selectedExecutable = settings.value("selected_executable").toInt();
  setExecutableIndex(selectedExecutable);
}


void MainWindow::storeSettings()
{
  if (m_CurrentProfile == NULL) {
    return;
  }
  m_CurrentProfile->writeModlist();
  m_CurrentProfile->createTweakedIniFile();
  saveCurrentLists();
  m_Settings.setupLoadMechanism();
  QSettings settings(ToQString(GameInfo::instance().getIniFilename()), QSettings::IniFormat);
  if (m_CurrentProfile != NULL) {
    settings.setValue("selected_profile", m_CurrentProfile->getName().toUtf8().constData());
  } else {
    settings.remove("selected_profile");
  }

  settings.setValue("mod_list_state", ui->modList->header()->saveState());

  settings.setValue("plugin_list_state", ui->espList->header()->saveState());
  settings.setValue("compact_downloads", ui->compactBox->isChecked());
  settings.setValue("ask_for_nexuspw", m_AskForNexusPW);

  settings.setValue("window_geometry", this->saveGeometry());
  settings.setValue("browser_geometry", m_NexusDialog.saveGeometry());

  settings.setValue("filters_visible", ui->displayCategoriesBtn->isChecked());

  settings.remove("customExecutables");
  settings.beginWriteArray("customExecutables");
  std::vector<Executable>::const_iterator current, end;
  m_ExecutablesList.getExecutables(current, end);
  int count = 0;
  for (; current != end; ++current) {
    const Executable &item = *current;
    if (item.m_Custom || item.m_Toolbar) {
      settings.setArrayIndex(count++);
      settings.setValue("binary", item.m_BinaryInfo.absoluteFilePath());
      settings.setValue("title", item.m_Title);
      settings.setValue("arguments", item.m_Arguments);
      settings.setValue("workingDirectory", item.m_WorkingDirectory);
      settings.setValue("closeOnStart", item.m_CloseMO == DEFAULT_CLOSE);
      settings.setValue("steamAppID", item.m_SteamAppID);
      settings.setValue("custom", item.m_Custom);
      settings.setValue("toolbar", item.m_Toolbar);
    }
  }
  settings.endArray();

  QComboBox *executableBox = findChild<QComboBox*>("executablesListBox");
  settings.setValue("selected_executable", executableBox->currentIndex());

  FileDialogMemory::save(m_Settings.directInterface());
}


void MainWindow::on_btnRefreshData_clicked()
{
  if (!m_DirectoryUpdate) {
    refreshDirectoryStructure();
    refreshDataTree();
  } else {
    qDebug("directory update");
  }
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
  saveCurrentLists();
  if (index == 0) {
    refreshESPList();
  } else if (index == 1) {
    refreshBSAList();
  } else if (index == 2) {
    refreshDataTree();
  } else if (index == 3) {
    refreshSaveList();
  } else if (index == 4) {
    ui->downloadView->scrollToBottom();
  }
}


static QString guessModName(const QString &fileName)
{
  return QFileInfo(fileName).baseName();
}


void MainWindow::installMod(const QString &fileName)
{
  bool hasIniTweaks = false;
  QString modName;
  m_CurrentProfile->writeModlistNow();
  if (m_InstallationManager.install(fileName, m_CurrentProfile->getPluginsFileName(), m_Settings.getModDirectory(), m_Settings.preferIntegratedInstallers(),
                                    m_Settings.enableQuickInstaller(), modName, hasIniTweaks)) {
    MessageDialog::showMessage(tr("Installation successful"), this);
    refreshModList();

    QModelIndexList posList = m_ModList.match(m_ModList.index(0, 0), Qt::DisplayRole, modName);
    if (posList.count() == 1) {
      ui->modList->scrollTo(posList.at(0));
    }
    int modIndex = ModInfo::getIndex(modName);
    if (modIndex != UINT_MAX) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
      if (hasIniTweaks &&
          (QMessageBox::question(this, tr("Configure Mod"),
              tr("This mod contains ini tweaks. Do you want to configure them now?"),
              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
        displayModInformation(modInfo, modIndex, ModInfoDialog::TAB_INIFILES);
      }
      testExtractBSA(modIndex);
    } else {
      reportError(tr("mod \"%1\" not found").arg(modName));
    }
  } else if (m_InstallationManager.wasCancelled()) {
    QMessageBox::information(this, tr("Installation cancelled"), tr("The mod was not installed completely."), QMessageBox::Ok);
  }
}


void MainWindow::installMod()
{
  try {
    QStringList extensions = m_InstallationManager.getSupportedExtensions();
    for (auto iter = extensions.begin(); iter != extensions.end(); ++iter) {
      *iter = "*." + *iter;
    }

    QString fileName = FileDialogMemory::getOpenFileName("installMod", this, tr("Choose Mod"), QString(),
                                                         tr("Mod Archive").append(QString(" (%1)").arg(extensions.join(" "))));

    if (fileName.length() == 0) {
      return;
    } else {
      installMod(fileName);
    }
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}


void MainWindow::on_btnSave_clicked()
{
  saveCurrentLists();
}


void MainWindow::on_startButton_clicked()
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");

  Executable selectedExecutable = executablesList->itemData(executablesList->currentIndex()).value<Executable>();

  spawnBinary(selectedExecutable.m_BinaryInfo,
              selectedExecutable.m_Arguments,
              selectedExecutable.m_WorkingDirectory.length() != 0 ? selectedExecutable.m_WorkingDirectory
                                                                  : selectedExecutable.m_BinaryInfo.absolutePath(),
              selectedExecutable.m_CloseMO == DEFAULT_CLOSE,
              selectedExecutable.m_SteamAppID);
}


static HRESULT CreateShortcut(LPCWSTR targetFileName, LPCWSTR arguments,
                              LPCSTR linkFileName, LPCWSTR description,
                              LPCWSTR currentDirectory)
{
  HRESULT result = E_INVALIDARG;
  if ((targetFileName != NULL) && (wcslen(targetFileName) > 0) &&
       (arguments != NULL) &&
       (linkFileName != NULL) && (strlen(linkFileName) > 0) &&
       (description != NULL) &&
       (currentDirectory != NULL)) {

    IShellLink* shellLink;
    result = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                              IID_IShellLink, (LPVOID*)&shellLink);

    if (!SUCCEEDED(result)) {
      qCritical("failed to create IShellLink instance");
      return result;
    }
      if (!SUCCEEDED(result)) return result;

    result = shellLink->SetPath(targetFileName);
    if (!SUCCEEDED(result)) {
      qCritical("failed to set target path %ls", targetFileName);
      shellLink->Release();
      return result;
    }
    result = shellLink->SetArguments(arguments);
    if (!SUCCEEDED(result)) {
      qCritical("failed to set arguments: %ls", arguments);
      shellLink->Release();
      return result;
    }

    if (wcslen(description) > 0) {
      result = shellLink->SetDescription(description);
      if (!SUCCEEDED(result)) {
        qCritical("failed to set description: %ls", description);
        shellLink->Release();
        return result;
      }
    }

    if (wcslen(currentDirectory) > 0) {
      result = shellLink->SetWorkingDirectory(currentDirectory);
      if (!SUCCEEDED(result)) {
        qCritical("failed to set working directory: %ls", currentDirectory);
        shellLink->Release();
        return result;
      }
    }

    IPersistFile* persistFile;
    result = shellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&persistFile);
    if (SUCCEEDED(result)) {
      wchar_t linkFileNameW[MAX_PATH];
      MultiByteToWideChar(CP_ACP, 0, linkFileName, -1, linkFileNameW, MAX_PATH);
      result = persistFile->Save(linkFileNameW, TRUE);
      persistFile->Release();
    } else {
      qCritical("failed to create IPersistFile instance");
    }

    shellLink->Release();
  }
  return result;
}


bool MainWindow::modifyExecutablesDialog()
{
  bool result = false;
  try {
    EditExecutablesDialog dialog(m_ExecutablesList);
    if (dialog.exec() == QDialog::Accepted) {
      m_ExecutablesList = dialog.getExecutablesList();
      result = true;
    }
    refreshExecutablesList();
  } catch (const std::exception &e) {
    reportError(e.what());
  }
  return result;
}

void MainWindow::on_executablesListBox_currentIndexChanged(int index)
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");

  int previousIndex = m_OldExecutableIndex;
  m_OldExecutableIndex = index;

  if (executablesList->isEnabled()) {

    if (executablesList->itemData(index).isNull()) {
      if (modifyExecutablesDialog()) {
        setExecutableIndex(previousIndex);
//        executablesList->setCurrentIndex(previousIndex);
      }
    } else {
      setExecutableIndex(index);
    }
  }
}

void MainWindow::helpTriggered()
{
  QWhatsThis::enterWhatsThisMode();
}

void MainWindow::wikiTriggered()
{
//  ::ShellExecuteW(NULL, L"open", L"http://issue.tannin.eu/tbg/wiki/Modorganizer%3AMainPage", NULL, NULL, SW_SHOWNORMAL);
  ::ShellExecuteW(NULL, L"open", L"http://wiki.step-project.com/Guide:Mod_Organizer", NULL, NULL, SW_SHOWNORMAL);
}

void MainWindow::issueTriggered()
{
  ::ShellExecuteW(NULL, L"open", L"http://issue.tannin.eu/tbg", NULL, NULL, SW_SHOWNORMAL);
}


void MainWindow::tutorialTriggered()
{
  QAction *tutorialAction = qobject_cast<QAction*>(sender());
  if (tutorialAction != NULL) {
    if (QMessageBox::question(this, tr("Start Tutorial?"),
          tr("You're about to start a tutorial. For technical reasons it's not possible to end "
             "the tutorial early. Continue?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      TutorialManager::instance().activateTutorial("MainWindow", tutorialAction->data().toString());
    }
  }
}


void MainWindow::on_actionInstallMod_triggered()
{
  installMod();
}

void MainWindow::on_actionAdd_Profile_triggered()
{
  bool repeat = true;
  while (repeat) {
    ProfilesDialog profilesDialog(m_GamePath);
    profilesDialog.exec();
    if (refreshProfiles() && !profilesDialog.failed()) {
      repeat = false;
    }
  }
//  addProfile();
}

void MainWindow::on_actionModify_Executables_triggered()
{
  if (modifyExecutablesDialog()) {
    setExecutableIndex(m_OldExecutableIndex);
  }
}


void MainWindow::setModListSorting(int index)
{
  Qt::SortOrder order = ((index & 0x01) != 0) ? Qt::DescendingOrder : Qt::AscendingOrder;
  int column = index >> 1;
  ui->modList->header()->setSortIndicator(column, order);
}


void MainWindow::setESPListSorting(int index)
{
  switch (index) {
    case 0: {
      ui->espList->header()->setSortIndicator(1, Qt::AscendingOrder);
    } break;
    case 1: {
      ui->espList->header()->setSortIndicator(1, Qt::DescendingOrder);
    } break;
    case 2: {
      ui->espList->header()->setSortIndicator(0, Qt::AscendingOrder);
    } break;
    case 3: {
      ui->espList->header()->setSortIndicator(0, Qt::DescendingOrder);
    } break;
  }
}


void MainWindow::setCompactDownloads(bool compact)
{
  ui->compactBox->setChecked(compact);
}


bool MainWindow::queryLogin(QString &username, QString &password)
{
  CredentialsDialog dialog(this);
  int res = dialog.exec();
  if (dialog.neverAsk()) {
    m_AskForNexusPW = false;
  }
  if (res == QDialog::Accepted) {
    username = dialog.username();
    password = dialog.password();
    if (dialog.store()) {
      m_Settings.setNexusLogin(username, password);
    }
    return true;
  } else {
    return false;
  }
}


bool MainWindow::setCurrentProfile(int index)
{
  QComboBox *profilesBox = findChild<QComboBox*>("profileBox");
  if (index >= profilesBox->count()) {
    return false;
  } else {
    profilesBox->setCurrentIndex(index);
    return true;
  }
}

bool MainWindow::setCurrentProfile(const QString &name)
{
  QComboBox *profilesBox = findChild<QComboBox*>("profileBox");
  for (int i = 0; i < profilesBox->count(); ++i) {
    if (QString::compare(profilesBox->itemText(i), name, Qt::CaseInsensitive) == 0) {
      profilesBox->setCurrentIndex(i);
      return true;
    }
  }
  // profile not valid
  profilesBox->setCurrentIndex(1);
  return false;
}


void MainWindow::esplist_changed()
{
  findChild<QPushButton*>("btnSave")->setEnabled(true);
}


void MainWindow::refresher_progress(int percent)
{
  m_RefreshProgress->setValue(percent);
}


void MainWindow::directory_refreshed()
{
  DirectoryEntry *newStructure = m_DirectoryRefresher.getDirectoryStructure();
  if (newStructure != NULL) {
    delete m_DirectoryStructure;
    m_DirectoryStructure = newStructure;
  } else {
    // TODO: don't know why this happens, this slot seems to get called twice with only one emit
    return;
  }
  m_DirectoryUpdate = false;
  refreshLists();
//  m_RefreshProgress->setVisible(false);
  statusBar()->hide();
}


void MainWindow::externalMessage(const QString &message)
{
  if (message.left(6).toLower() == "nxm://") {
    MessageDialog::showMessage(tr("Download started"), this);
    downloadRequestedNXM(message);
  }
}


void MainWindow::modStatusChanged(unsigned int index)
{
  try {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
    if (m_CurrentProfile->modEnabled(index)) {
      DirectoryRefresher::addModToStructure(m_DirectoryStructure, modInfo->name(), m_CurrentProfile->getModPriority(index), modInfo->absolutePath());
/*      m_DirectoryStructure->addFromOrigin(ToWString(modInfo->name()),
          ToWString(QDir::toNativeSeparators(modInfo->absolutePath())),
          m_CurrentProfile->getModPriority(index));*/
      DirectoryRefresher::cleanStructure(m_DirectoryStructure);
    } else {
      if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
        FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
        origin.enable(false);
      }
    }
    refreshLists();
  } catch (const std::exception& e) {
    reportError(tr("failed to update mod list: %1").arg(e.what()));
  }
}


void MainWindow::removeOrigin(const QString &name)
{
  FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(name));
  origin.enable(false);
  refreshLists();
}


void MainWindow::modorder_changed()
{
  for (unsigned int i = 0; i < m_CurrentProfile->numMods(); ++i) {
    int priority = m_CurrentProfile->getModPriority(i);
    if (m_CurrentProfile->modEnabled(i)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      m_DirectoryStructure->getOriginByName(ToWString(modInfo->name())).setPriority(priority);
    }
  }
  m_DirectoryStructure->getFileRegister()->sortOrigins();
}

void MainWindow::procError(QProcess::ProcessError error)
{
  reportError(tr("failed to spawn notepad.exe: %1").arg(error));
  this->sender()->deleteLater();
}

void MainWindow::procFinished(int, QProcess::ExitStatus)
{
  this->sender()->deleteLater();
}


void MainWindow::on_profileRefreshBtn_clicked()
{
  m_CurrentProfile->writeModlist();

//  m_ModList.updateModCollection();
  refreshModList();
}


void MainWindow::showMessage(const QString &message)
{
  MessageDialog::showMessage(message, this);
}


void MainWindow::showError(const QString &message)
{
  reportError(message);
}


void MainWindow::installMod_clicked()
{
  installMod();
}


void MainWindow::renameModInList(QFile &modList, const QString &oldName, const QString &newName)
{
  //TODO this code needs to be merged with ModList::readFrom
  if (!modList.open(QIODevice::ReadWrite)) {
    reportError(tr("failed to open %1").arg(modList.fileName()));
    return;
  }

  QBuffer outBuffer;
  outBuffer.open(QIODevice::WriteOnly);

  while (!modList.atEnd()) {
    QByteArray line = modList.readLine();
    if (line.length() == 0) {
      break;
    }
    if (line.at(0) == '#') {
      outBuffer.write(line);
      continue;
    }
    QString modName = QString::fromUtf8(line.constData()).trimmed();
    if (modName.isEmpty()) {
      break;
    }

    bool disabled = false;
    if (modName.at(0) == '-') {
      disabled = true;
      modName = modName.mid(1);
    } else if (modName.at(0) == '+') {
      modName = modName.mid(1);
    }

    if (modName == oldName) {
      modName = newName;
    }

    if (disabled) {
      outBuffer.write("-");
    } else {
      outBuffer.write("+");
    }
    outBuffer.write(modName.toUtf8().constData());
    outBuffer.write("\r\n");
  }

  modList.resize(0);
  modList.write(outBuffer.buffer());
  modList.close();
}


void MainWindow::modRenamed(const QString &oldName, const QString &newName)
{
  // fix the profiles directly on disc
  for (int i = 0; i < ui->profileBox->count(); ++i) {
    QString profileName = ui->profileBox->itemText(i);

    //TODO this functionality should be in the Profile class
    QString modlistName = QString("%1/%2/modlist.txt")
                            .arg(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getProfilesDir())))
                            .arg(profileName);

    QFile modList(modlistName);
    if (modList.exists()) {
      qDebug("rewrite modlist %s", QDir::toNativeSeparators(modlistName).toUtf8().constData());
      renameModInList(modList, oldName, newName);
    }
  }

  // immediately refresh the active profile because the data in memory is invalid
  m_CurrentProfile->refreshModStatus();

  // also fix the directory structure
  try {
    if (m_DirectoryStructure->originExists(ToWString(oldName))) {
      FilesOrigin &origin = m_DirectoryStructure->getOriginByName(ToWString(oldName));
      origin.setName(ToWString(newName));
    } else {

    }
  } catch (const std::exception &e) {
    reportError(tr("failed to change origin name: %1").arg(e.what()));
  }
}


QTreeWidgetItem *MainWindow::addFilterItem(QTreeWidgetItem *root, const QString &name, int categoryID)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(name));
  item->setData(0, Qt::UserRole, categoryID);
  if (root != NULL) {
    root->addChild(item);
  } else {
    ui->categoriesList->addTopLevelItem(item);
  }
  return item;
}


void MainWindow::addCategoryFilters(QTreeWidgetItem *root, const std::set<int> &categoriesUsed, int targetID)
{
  for (unsigned i = 1; i < m_CategoryFactory.numCategories(); ++i) {
    if ((m_CategoryFactory.getParentID(i) == targetID)) {
      int categoryID = m_CategoryFactory.getCategoryID(i);
      if (categoriesUsed.find(categoryID) != categoriesUsed.end()) {
        QTreeWidgetItem *item = addFilterItem(root, m_CategoryFactory.getCategoryName(i), categoryID);
        if (m_CategoryFactory.hasChildren(i)) {
          addCategoryFilters(item, categoriesUsed, categoryID);
        }
      }
    }
  }
}


void MainWindow::refreshFilters()
{
  ui->modList->setCurrentIndex(QModelIndex());

  // save previous filter text so we can restore it later, in case the filter still exists then
  QTreeWidgetItem *currentItem = ui->categoriesList->currentItem();
  QString previousFilter = currentItem != NULL ? currentItem->text(0) : tr("<All>");

  ui->categoriesList->clear();
  addFilterItem(NULL, tr("<All>"), CategoryFactory::CATEGORY_NONE);
  addFilterItem(NULL, tr("<Checked>"), CategoryFactory::CATEGORY_SPECIAL_CHECKED);
  addFilterItem(NULL, tr("<Unchecked>"), CategoryFactory::CATEGORY_SPECIAL_UNCHECKED);
  addFilterItem(NULL, tr("<Update>"), CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE);
  addFilterItem(NULL, tr("<No category>"), CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY);
  addFilterItem(NULL, tr("<Conflicted>"), CategoryFactory::CATEGORY_SPECIAL_CONFLICT);

  std::set<int> categoriesUsed;
  for (unsigned int modIdx = 0; modIdx < ModInfo::getNumMods(); ++modIdx) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIdx);
    BOOST_FOREACH (int categoryID, modInfo->getCategories()) {
      int currentID = categoryID;
      // also add parents so they show up in the tree
      while (currentID != 0) {
        categoriesUsed.insert(currentID);
        currentID = m_CategoryFactory.getParentID(m_CategoryFactory.getCategoryIndex(currentID));
      }
    }
  }

  addCategoryFilters(NULL, categoriesUsed, 0);

  QList<QTreeWidgetItem*> matches = ui->categoriesList->findItems(previousFilter, Qt::MatchFixedString | Qt::MatchRecursive);
  if (matches.size() > 0) {
    QTreeWidgetItem *currentItem = matches.at(0);
    ui->categoriesList->setCurrentItem(currentItem);
    while (currentItem != NULL) {
      currentItem->setExpanded(true);
      currentItem = currentItem->parent();
    }
  }
}


void MainWindow::renameMod_clicked()
{
  try {
    QModelIndex treeIdx = m_ModListSortProxy->mapFromSource(m_ModList.index(m_ContextRow, 0));
    ui->modList->setCurrentIndex(treeIdx);
    ui->modList->edit(treeIdx);
  } catch (const std::exception &e) {
    reportError(tr("failed to rename mod: %1").arg(e.what()));
  }
}


void MainWindow::restoreBackup_clicked()
{
  QRegExp backupRegEx("(.*)_backup[0-9]*$");
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  if (backupRegEx.indexIn(modInfo->name()) != -1) {
    QString regName = backupRegEx.cap(1);
    QDir modDir(QDir::fromNativeSeparators(m_Settings.getModDirectory()));
    if (!modDir.exists(regName) ||
        (QMessageBox::question(this, tr("Overwrite?"),
          tr("This will replace the existing mod \"%1\". Continue?").arg(regName),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
      if (modDir.exists(regName) && !removeDir(modDir.absoluteFilePath(regName))) {
        reportError(tr("failed to remove mod \"%1\"").arg(regName));
      } else {
        QString destinationPath = QDir::fromNativeSeparators(m_Settings.getModDirectory()) + "/" + regName;
        if (!modDir.rename(modInfo->absolutePath(), destinationPath)) {
          reportError(tr("failed to rename \"%1\" to \"%2\"").arg(modInfo->absolutePath()).arg(destinationPath));
        }
        refreshModList();
      }
    }
  }
}


void MainWindow::removeMod_clicked()
{
  try {
    QItemSelectionModel *selection = ui->modList->selectionModel();
    if (selection->hasSelection() && selection->selectedRows().count() > 1  ) {
      QString mods;
      QStringList modNames;
      foreach (QModelIndex idx, selection->selectedRows()) {
        QString name = ModInfo::getByIndex(m_ModListSortProxy->mapToSource(idx).row())->name();
        mods += "<li>" + name + "</li>";
        modNames.append(name);
      }
      if (QMessageBox::question(this, tr("Confirm"),
                                tr("Remove the following mods?<br><ul>%1</ul>").arg(mods),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        // use mod names instead of indexes because those become invalid during the removal
        foreach (QString name, modNames) {
          m_ModList.removeRowForce(ModInfo::getIndex(name));
        }
      }
    } else {
      m_ModList.removeRow(m_ContextRow, QModelIndex());
    }
  } catch (const std::exception &e) {
    reportError(tr("failed to remove mod: %1").arg(e.what()));
  }
}


void MainWindow::reinstallMod_clicked()
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  QString installationFile = modInfo->getInstallationFile();
  if (installationFile.length() != 0) {
    // there was a bug where mods installed through NCC had the absolute download path stored
    if (QFileInfo(installationFile).isAbsolute()) {
      installationFile = QFileInfo(installationFile).fileName();
    }
    QString fullInstallationFile = m_DownloadManager.getOutputDirectory().append("/").append(installationFile);
    if (QFile::exists(fullInstallationFile)) {
      installMod(fullInstallationFile);
    } else {
      QMessageBox::information(this, tr("Failed"), tr("Installation file no longer exists"));
    }
  } else {
    QMessageBox::information(this, tr("Failed"),
                             tr("Mods installed with old versions of MO can't be reinstalled in this way."));
  }
}


void MainWindow::endorseMod(ModInfo::Ptr mod)
{
  if (NexusInterface::instance()->getAccessManager()->loggedIn()) {
    mod->endorse(true);
  } else {
    QString username, password;
    if (m_Settings.getNexusLogin(username, password)) {
      m_PostLoginTasks.push_back(boost::bind(&MainWindow::endorseMod, _1, mod));
      m_NexusDialog.login(username, password);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
    }
  }
}


void MainWindow::endorse_clicked()
{
  endorseMod(ModInfo::getByIndex(m_ContextRow));
}


void MainWindow::unendorse_clicked()
{
  QString username, password;
  if (NexusInterface::instance()->getAccessManager()->loggedIn()) {
    ModInfo::getByIndex(m_ContextRow)->endorse(false);
  } else {
    if (m_Settings.getNexusLogin(username, password)) {
      m_PostLoginTasks.push_back(boost::mem_fn(&MainWindow::unendorse_clicked));
      m_NexusDialog.login(username, password);
    } else {
      MessageDialog::showMessage(tr("You need to be logged in with Nexus to endorse"), this);
    }
  }
}


void MainWindow::displayModInformation(ModInfo::Ptr modInfo, unsigned int index, int tab)
{
  std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
    OverwriteInfoDialog dialog(modInfo, this);
    dialog.exec();
  } else {
    ModInfoDialog dialog(modInfo, m_DirectoryStructure, this);
    connect(&dialog, SIGNAL(nexusLinkActivated(QString)), this, SLOT(nexusLinkActivated(QString)));
    connect(&dialog, SIGNAL(downloadRequest(QString)), this, SLOT(downloadRequestedNXM(QString)));
    connect(&dialog, SIGNAL(modOpen(QString, int)), this, SLOT(displayModInformation(QString, int)));
    connect(&dialog, SIGNAL(originModified(int)), this, SLOT(originModified(int)));
    connect(&dialog, SIGNAL(endorseMod(ModInfo::Ptr)), this, SLOT(endorseMod(ModInfo::Ptr)));

    dialog.openTab(tab);
    dialog.exec();

    modInfo->saveMeta();
    emit modInfoDisplayed();
  }

  if (m_CurrentProfile->modEnabled(index)) {
    FilesOrigin& origin = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
    origin.enable(false);

    if (m_DirectoryStructure->originExists(ToWString(modInfo->name()))) {
      FilesOrigin& origin = m_DirectoryStructure->getOriginByName(ToWString(modInfo->name()));
      origin.enable(false);

/*      m_DirectoryStructure->addFromOrigin(ToWString(modInfo->name()),
                                          ToWString(QDir::toNativeSeparators(modInfo->absolutePath())),
                                          m_CurrentProfile->getModPriority(index));*/
      DirectoryRefresher::addModToStructure(m_DirectoryStructure,
                                            modInfo->name(), m_CurrentProfile->getModPriority(index),
                                            modInfo->absolutePath());
      DirectoryRefresher::cleanStructure(m_DirectoryStructure);
      refreshLists();
    }
  }
}


void MainWindow::displayModInformation(const QString &modName, int tab)
{
  unsigned int index = ModInfo::getIndex(modName);
  if (index == UINT_MAX) {
    qCritical("failed to resolve mod name %s", modName.toUtf8().constData());
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  displayModInformation(modInfo, index, tab);
}


void MainWindow::displayModInformation(int row, int tab)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(row);
  displayModInformation(modInfo, row, tab);
}


void MainWindow::testExtractBSA(int modIndex)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
  QDir dir(modInfo->absolutePath());

  QFileInfoList archives = dir.entryInfoList(QStringList("*.bsa"));
  if (archives.length() != 0 &&
      (QuestionBoxMemory::query(this, m_Settings.directInterface(), "unpackBSA", tr("Extract BSA"),
                             tr("This mod contains at least one BSA. Do you want to unpack it?\n"
                                "(This removes the BSA after completion. If you don't know about BSAs, just select no)"),
                             QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No) == QMessageBox::Yes)) {


    foreach (QFileInfo archiveInfo, archives) {
      BSA::Archive archive;

      BSA::EErrorCode result = archive.read(archiveInfo.absoluteFilePath().toLocal8Bit().constData());
      if ((result != BSA::ERROR_NONE) && (result != BSA::ERROR_INVALIDHASHES)) {
        reportError(tr("failed to read %1: %2").arg(archiveInfo.fileName()).arg(result));
        return;
      }

      QProgressDialog progress(this);
      progress.setMaximum(100);
      progress.setValue(0);
      progress.show();

      archive.extractAll(modInfo->absolutePath().toUtf8().constData(),
                         boost::bind(&MainWindow::extractProgress, this, boost::ref(progress), _1, _2));

      if (result == BSA::ERROR_INVALIDHASHES) {
        reportError(tr("This archive contains invalid hashes. Some files may be broken."));
      }

      archive.close();

      if (!QFile::remove(archiveInfo.absoluteFilePath())) {
        qCritical("failed to remove archive %s", archiveInfo.absoluteFilePath().toUtf8().constData());
      } else {
        m_DirectoryStructure->removeFile(ToWString(archiveInfo.fileName()));
      }
    }

    refreshBSAList();
  }
}


void MainWindow::visitOnNexus_clicked()
{
  int modID = m_ModList.data(m_ModList.index(m_ContextRow, 0), Qt::UserRole).toInt();
  if (modID > 0)  {
    nexusLinkActivated(QString("%1/downloads/file.php?id=%2").arg(ToQString(GameInfo::instance().getNexusPage())).arg(modID));
  } else {
    MessageDialog::showMessage(tr("Nexus ID for this Mod is unknown"), this);
  }
}

void MainWindow::openExplorer_clicked()
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);

  ::ShellExecuteW(NULL, L"explore", ToWString(modInfo->absolutePath()).c_str(), NULL, NULL, SW_SHOWNORMAL);
}


void MainWindow::information_clicked()
{
  try {
    displayModInformation(m_ContextRow);
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}


void MainWindow::syncOverwrite()
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  SyncOverwriteDialog syncDialog(modInfo->absolutePath(), m_DirectoryStructure, this);
  if (syncDialog.exec() == QDialog::Accepted) {
    syncDialog.apply(QDir::fromNativeSeparators(m_Settings.getModDirectory()));
    modInfo->testValid();
    refreshDirectoryStructure();
  }
}


void MainWindow::setPriorityMax()
{
  int newPriority = m_CurrentProfile->numMods() - 1;
  m_CurrentProfile->setModPriority(m_ContextRow, newPriority);
}


void MainWindow::setPriorityManually()
{
//  m_CurrentProfile->setModPriority(m_ContextRow, m_CurrentProfile->numMods() - 1);

  int current = m_CurrentProfile->getModPriority(m_ContextRow);
  int newPriority = QInputDialog::getInt(this, tr("Priority"), tr("Choose Priority"), current, 0, m_ModList.rowCount() - 1);
  m_ModList.changeModPriority(m_ContextRow, newPriority);
//  m_CurrentProfile->setModPriority(m_ContextRow, newPriority);
}


void MainWindow::setPriorityMin()
{
  int newPriority = 0;
  m_CurrentProfile->setModPriority(m_ContextRow, newPriority);
}


void MainWindow::cancelModListEditor()
{
  ui->modList->setEnabled(false);
  ui->modList->setEnabled(true);
}


void MainWindow::on_modList_doubleClicked(const QModelIndex &index)
{
  try {
    displayModInformation(m_ModListSortProxy->mapToSource(index).row());
    // workaround to cancel the editor that might have opened because of
    // selection-click
    ui->modList->closePersistentEditor(index);
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}


bool MainWindow::addCategories(QMenu *menu, int targetID)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  const std::set<int> &categories = modInfo->getCategories();

  bool childEnabled = false;

  for (unsigned i = 1; i < m_CategoryFactory.numCategories(); ++i) {
    if (m_CategoryFactory.getParentID(i) == targetID) {
      QMenu *targetMenu = menu;
      if (m_CategoryFactory.hasChildren(i)) {
        targetMenu = menu->addMenu(
            m_CategoryFactory.getCategoryName(i).replace('&', "&&"));
      }

      int id = m_CategoryFactory.getCategoryID(i);
      QScopedPointer<QCheckBox> checkBox(new QCheckBox(targetMenu));
      bool enabled = categories.find(id) != categories.end();
      checkBox->setText(m_CategoryFactory.getCategoryName(i).replace('&', "&&"));
      if (enabled) {
        childEnabled = true;
      }
      checkBox->setChecked(enabled ? Qt::Checked : Qt::Unchecked);

      QScopedPointer<QWidgetAction> checkableAction(new QWidgetAction(targetMenu));
      checkableAction->setDefaultWidget(checkBox.take());
      checkableAction->setData(id);
      targetMenu->addAction(checkableAction.take());

      if (m_CategoryFactory.hasChildren(i)) {
        if (addCategories(targetMenu, m_CategoryFactory.getCategoryID(i)) || enabled) {
          targetMenu->setIcon(QIcon(":/MO/gui/resources/check.png"));
        }
      }
    }
  }
  return childEnabled;
}


void MainWindow::saveCategoriesFromMenu(QMenu *menu, int modRow)
{
  ModInfo::Ptr modInfo = ModInfo::getByIndex(modRow);
  foreach (QAction* action, menu->actions()) {
    if (action->menu() != NULL) {
      saveCategoriesFromMenu(action->menu(), modRow);
    } else {
      QWidgetAction *widgetAction = qobject_cast<QWidgetAction*>(action);
      if (widgetAction != NULL) {
        QCheckBox *checkbox = qobject_cast<QCheckBox*>(widgetAction->defaultWidget());
        modInfo->setCategory(widgetAction->data().toInt(), checkbox->isChecked());
//        m_ModList.setModCategory(modRow, widgetAction->data().toInt(), checkbox->isChecked());
      }
    }
  }
}


void MainWindow::saveCategories()
{
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == NULL) {
    qCritical("not a menu?");
    return;
  }
//  m_ModList.resetCategories(m_ContextRow);
  saveCategoriesFromMenu(menu, m_ContextRow);

  refreshFilters();
}


void MainWindow::savePrimaryCategory()
{
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == NULL) {
    qCritical("not a menu?");
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);
  foreach (QAction* action, menu->actions()) {
    QWidgetAction *widgetAction = qobject_cast<QWidgetAction*>(action);
    if (widgetAction != NULL) {
      QRadioButton *btn = qobject_cast<QRadioButton*>(widgetAction->defaultWidget());
      if (btn->isChecked()) {
        modInfo->setPrimaryCategory(widgetAction->data().toInt());
        break;
      }
    }
  }
}


void MainWindow::checkModsForUpdates()
{
  statusBar()->show();
  if (NexusInterface::instance()->getAccessManager()->loggedIn()) {
    m_ModsToUpdate = ModInfo::checkAllForUpdate(this);
    m_RefreshProgress->setRange(0, m_ModsToUpdate);
  } else {
    QString username, password;
    if (m_Settings.getNexusLogin(username, password)) {
      m_PostLoginTasks.push_back(boost::mem_fn(&MainWindow::checkModsForUpdates));
      m_NexusDialog.login(username, password);
    } else { // otherwise there will be no endorsement info
      m_ModsToUpdate = ModInfo::checkAllForUpdate(this);
    }
  }
}


void MainWindow::addPrimaryCategoryCandidates(QMenu *primaryCategoryMenu, ModInfo::Ptr info)
{
  const std::set<int> &categories = info->getCategories();
  foreach (int categoryID, categories) {
    int catIdx = m_CategoryFactory.getCategoryIndex(categoryID);
    QWidgetAction *action = new QWidgetAction(primaryCategoryMenu);
    try {
      QRadioButton *categoryBox = new QRadioButton(m_CategoryFactory.getCategoryName(catIdx).replace('&', "&&"),
                                             primaryCategoryMenu);
      categoryBox->setChecked(categoryID == info->getPrimaryCategory());
      action->setDefaultWidget(categoryBox);
    } catch (const std::exception &e) {
      qCritical("failed to create category checkbox: %s", e.what());
    }

    action->setData(categoryID);
    primaryCategoryMenu->addAction(action);
  }
}


void MainWindow::addPrimaryCategoryCandidates()
{
  QMenu *menu = qobject_cast<QMenu*>(sender());
  if (menu == NULL) {
    qCritical("not a menu?");
    return;
  }
  menu->clear();
  ModInfo::Ptr modInfo = ModInfo::getByIndex(m_ContextRow);

  addPrimaryCategoryCandidates(menu, modInfo);
}


void MainWindow::enableVisibleMods()
{
  if (QMessageBox::question(NULL, tr("Confirm"), tr("Really enable all visible mods?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_ModListSortProxy->enableAllVisible();
  }
}


void MainWindow::disableVisibleMods()
{
  if (QMessageBox::question(NULL, tr("Confirm"), tr("Really disable all visible mods?"),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_ModListSortProxy->disableAllVisible();
  }
}


void MainWindow::exportModListCSV()
{
  SelectionDialog selection(tr("Choose what to export"));

  selection.addChoice(tr("Everything"), tr("All installed mods are included in the list"), 0);
  selection.addChoice(tr("Active Mods"), tr("Only active (checked) mods from your current profile are included"), 1);
  selection.addChoice(tr("Visible"), tr("All mods visible in the mod list are included"), 2);

  if (selection.exec() == QDialog::Accepted) {
    unsigned int numMods = ModInfo::getNumMods();

    try {
      QBuffer buffer;
      buffer.open(QIODevice::ReadWrite);
      CSVBuilder builder(&buffer);
      builder.setEscapeMode(CSVBuilder::TYPE_STRING, CSVBuilder::QUOTE_ALWAYS);
      std::vector<std::pair<QString, CSVBuilder::EFieldType> > fields;
      fields.push_back(std::make_pair(QString("mod_id"), CSVBuilder::TYPE_INTEGER));
      fields.push_back(std::make_pair(QString("mod_installed_name"), CSVBuilder::TYPE_STRING));
      fields.push_back(std::make_pair(QString("mod_version"), CSVBuilder::TYPE_STRING));
      fields.push_back(std::make_pair(QString("file_installed_name"), CSVBuilder::TYPE_STRING));
      builder.setFields(fields);

      builder.writeHeader();

      for (unsigned int i = 0; i < numMods; ++i) {
        ModInfo::Ptr info = ModInfo::getByIndex(i);
        bool enabled = m_CurrentProfile->modEnabled(i);
        if ((selection.getChoiceData().toInt() == 1) && !enabled) {
          continue;
        } else if ((selection.getChoiceData().toInt() == 2) && !m_ModListSortProxy->filterMatches(info, enabled)) {
          continue;
        }
        std::vector<ModInfo::EFlag> flags = info->getFlags();
        if ((std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) == flags.end()) &&
            (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) == flags.end())) {
          builder.setRowField("mod_id", info->getNexusID());
          builder.setRowField("mod_installed_name", info->name());
          builder.setRowField("mod_version", info->getVersion().canonicalString());
          builder.setRowField("file_installed_name", info->getInstallationFile());
          builder.writeRow();
        }
      }

      SaveTextAsDialog saveDialog(this);
      saveDialog.setText(buffer.data());
      saveDialog.exec();
    } catch (const std::exception &e) {
      reportError(tr("export failed: %1").arg(e.what()));
    }
  }
}


void MainWindow::on_modList_customContextMenuRequested(const QPoint &pos)
{
  try {
    QTreeView *modList = findChild<QTreeView*>("modList");

    m_ContextRow = m_ModListSortProxy->mapToSource(modList->indexAt(pos)).row();

    QMenu menu;
    menu.addAction(tr("Install Mod..."), this, SLOT(installMod_clicked()));

    menu.addAction(tr("Enable all visible"), this, SLOT(enableVisibleMods()));
    menu.addAction(tr("Disable all visible"), this, SLOT(disableVisibleMods()));

    menu.addAction(tr("Check all for update"), this, SLOT(checkModsForUpdates()));

    menu.addAction(tr("Refresh"), this, SLOT(on_profileRefreshBtn_clicked()));

    menu.addAction(tr("Export to csv..."), this, SLOT(exportModListCSV()));

    if (m_ContextRow != -1) {
      menu.addSeparator();
      ModInfo::Ptr info = ModInfo::getByIndex(m_ContextRow);
      std::vector<ModInfo::EFlag> flags = info->getFlags();
      if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end()) {
        menu.addAction(tr("Sync to Mods..."), this, SLOT(syncOverwrite()));
      } else if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) != flags.end()) {
        menu.addAction(tr("Restore Backup"), this, SLOT(restoreBackup_clicked()));
        menu.addAction(tr("Remove Backup..."), this, SLOT(removeMod_clicked()));
      } else {
        QMenu *addCategoryMenu = menu.addMenu(tr("Set Category"));
        addCategories(addCategoryMenu, 0);
        connect(addCategoryMenu, SIGNAL(aboutToHide()), this, SLOT(saveCategories()));

        QMenu *primaryCategoryMenu = menu.addMenu(tr("Primary Category"));
        connect(primaryCategoryMenu, SIGNAL(aboutToShow()), this, SLOT(addPrimaryCategoryCandidates()));
        connect(primaryCategoryMenu, SIGNAL(aboutToHide()), this, SLOT(savePrimaryCategory()));

        menu.addAction(tr("Rename Mod..."), this, SLOT(renameMod_clicked()));
        menu.addAction(tr("Remove Mod..."), this, SLOT(removeMod_clicked()));
        menu.addAction(tr("Reinstall Mod"), this, SLOT(reinstallMod_clicked()));
        switch (info->endorsedState()) {
          case ModInfo::ENDORSED_TRUE: {
            menu.addAction(tr("Un-Endorse"), this, SLOT(unendorse_clicked()));
          } break;
          case ModInfo::ENDORSED_FALSE: {
            menu.addAction(tr("Endorse"), this, SLOT(endorse_clicked()));
          } break;
          default: {
            QAction *action = new QAction(tr("Endorsement state unknown"), &menu);
            action->setEnabled(false);
            menu.addAction(action);
          } break;
        }

        menu.addAction(tr("Visit on Nexus"), this, SLOT(visitOnNexus_clicked()));
        menu.addAction(tr("Open in explorer"), this, SLOT(openExplorer_clicked()));
      }

      QAction *infoAction = menu.addAction(tr("Information..."), this, SLOT(information_clicked()));
      menu.setDefaultAction(infoAction);
    }

    menu.exec(modList->mapToGlobal(pos));
  } catch (const std::exception &e) {
    reportError(tr("Exception: ").arg(e.what()));
  } catch (...) {
    reportError(tr("Unknown exception"));
  }
}


void MainWindow::on_categoriesList_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *)
{
  if (current != NULL) {
    m_ModListSortProxy->setCategoryFilter(current->data(0, Qt::UserRole).toInt());
    ui->currentCategoryLabel->setText(QString("(%1)").arg(current->text(0)));
  }
}


void MainWindow::fixMods_clicked()
{
  QListWidgetItem *selectedItem = ui->savegameList->item(m_SelectedSaveGame);
  if (selectedItem == NULL) {
    return;
  }

  // if required, parse the save game
  if (selectedItem->data(Qt::UserRole).isNull()) {
    QVariant temp;
    SaveGameGamebryo *save = getSaveGame(selectedItem->data(Qt::UserRole).toString());
    save->setParent(selectedItem->listWidget());
    temp.setValue(save);
    selectedItem->setData(Qt::UserRole, temp);
  }

  const SaveGameGamebryo *save = getSaveGame(selectedItem);

  // collect the list of missing plugins
  std::map<QString, std::vector<QString> > missingPlugins;

  for (int i = 0; i < save->numPlugins(); ++i) {
    const QString &pluginName = save->plugin(i);
    if (!m_PluginList.isEnabled(pluginName)) {
      missingPlugins[pluginName] = std::vector<QString>();
    }
  }

  // figure out, for each esp/esm, which mod, if any, contains it
  QStringList espFilter("*.esp");
  espFilter.append("*.esm");

  {
    QDir dataDir(m_GamePath + "/data");
    QStringList esps = dataDir.entryList(espFilter);
    foreach (const QString &esp, esps) {
      std::map<QString, std::vector<QString> >::iterator iter = missingPlugins.find(esp);
      if (iter != missingPlugins.end()) {
        iter->second.push_back("<data>");
      }
    }
  }

  for (unsigned int i = 0; i < m_CurrentProfile->numRegularMods(); ++i) {
    int modIndex = m_CurrentProfile->modIndexByPriority(i);
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);

    QStringList esps = QDir(modInfo->absolutePath()).entryList(espFilter);
    foreach (const QString &esp, esps) {
      std::map<QString, std::vector<QString> >::iterator iter = missingPlugins.find(esp);
      if (iter != missingPlugins.end()) {
        iter->second.push_back(modInfo->name());
      }
    }
  }

  ActivateModsDialog dialog(missingPlugins, this);
  if (dialog.exec() == QDialog::Accepted) {
    // activate the required mods, then enable all esps
    std::set<QString> modsToActivate = dialog.getModsToActivate();
    for (std::set<QString>::iterator iter = modsToActivate.begin(); iter != modsToActivate.end(); ++iter) {
      if (*iter != "<data>") {
        unsigned int modIndex = ModInfo::getIndex(*iter);
        m_CurrentProfile->setModEnabled(modIndex, true);
      }
    }

    m_CurrentProfile->writeModlist();
    refreshLists();

    std::set<QString> espsToActivate = dialog.getESPsToActivate();
    for (std::set<QString>::iterator iter = espsToActivate.begin(); iter != espsToActivate.end(); ++iter) {
      m_PluginList.enableESP(*iter);
    }
    saveCurrentLists();
  }
}


void MainWindow::on_savegameList_customContextMenuRequested(const QPoint &pos)
{
  QListWidgetItem *selectedItem = ui->savegameList->itemAt(pos);
  if (selectedItem == NULL) {
    return;
  }

  m_SelectedSaveGame = ui->savegameList->row(selectedItem);

  QMenu menu;
  menu.addAction(tr("Fix Mods..."), this, SLOT(fixMods_clicked()));

  menu.exec(ui->savegameList->mapToGlobal(pos));
}

void MainWindow::linkToolbar()
{
  const Executable &selectedExecutable = ui->executablesListBox->itemData(ui->executablesListBox->currentIndex()).value<Executable>();
  Executable &exe = m_ExecutablesList.find(selectedExecutable.m_Title);
  exe.m_Toolbar = !exe.m_Toolbar;
  updateToolBar();
}

void MainWindow::linkDesktop()
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");
//  QPushButton *linkButton = findChild<QPushButton*>("linkDesktopButton");

  const Executable &selectedExecutable = executablesList->itemData(executablesList->currentIndex()).value<Executable>();
  QString linkName = getDesktopDirectory() + "\\" + selectedExecutable.m_Title + ".lnk";

  if (QFile::exists(linkName)) {
    if (QFile::remove(linkName)) {
      ui->linkButton->menu()->actions().at(0)->setIcon(QIcon(":/MO/gui/link"));
    } else {
      reportError(tr("failed to remove %1").arg(linkName));
    }
  } else {
    QFileInfo exeInfo(m_ExeName);
    // create link
    std::wstring targetFile       = ToWString(exeInfo.absoluteFilePath());
    std::wstring parameter        = ToWString(QString("\"") + selectedExecutable.m_BinaryInfo.absoluteFilePath() + "\" " + selectedExecutable.m_Arguments);
    std::wstring description      = ToWString(selectedExecutable.m_BinaryInfo.fileName());
    std::wstring currentDirectory = ToWString(selectedExecutable.m_BinaryInfo.absolutePath());

    if (CreateShortcut(targetFile.c_str(), parameter.c_str(),
                       linkName.toUtf8().constData(),
                       description.c_str(), currentDirectory.c_str()) != E_INVALIDARG) {
      ui->linkButton->menu()->actions().at(0)->setIcon(QIcon(":/MO/gui/remove"));
    } else {
      reportError(tr("failed to create %1").arg(linkName));
    }
  }
}

void MainWindow::linkMenu()
{
  QComboBox* executablesList = findChild<QComboBox*>("executablesListBox");

  const Executable &selectedExecutable = executablesList->itemData(executablesList->currentIndex()).value<Executable>();
  QString linkName = getStartMenuDirectory() + "\\" + selectedExecutable.m_Title + ".lnk";

  if (QFile::exists(linkName)) {
    if (QFile::remove(linkName)) {
      ui->linkButton->menu()->actions().at(1)->setIcon(QIcon(":/MO/gui/link"));
    } else {
      reportError(tr("failed to remove %1").arg(linkName));
    }
  } else {
    QFileInfo exeInfo(m_ExeName);
    // create link
    std::wstring targetFile       = ToWString(exeInfo.absoluteFilePath());
    std::wstring parameter        = ToWString(QString("\"") + selectedExecutable.m_BinaryInfo.absoluteFilePath() + "\" " + selectedExecutable.m_Arguments);
    std::wstring description      = ToWString(selectedExecutable.m_BinaryInfo.fileName());
    std::wstring currentDirectory = ToWString(selectedExecutable.m_BinaryInfo.absolutePath());

    if (CreateShortcut(targetFile.c_str(), parameter.c_str(),
                       linkName.toUtf8().constData(),
                       description.c_str(), currentDirectory.c_str()) != E_INVALIDARG) {
      ui->linkButton->menu()->actions().at(1)->setIcon(QIcon(":/MO/gui/remove"));
    } else {
      reportError(tr("failed to create %1").arg(linkName));
    }
  }
}

void MainWindow::on_actionSettings_triggered()
{
  QString oldModDirectory(m_Settings.getModDirectory());
  QString oldCacheDirectory(m_Settings.getCacheDirectory());
  m_Settings.query(this);
  fixCategories();
  refreshFilters();
  if (QDir::fromNativeSeparators(m_DownloadManager.getOutputDirectory()) != QDir::fromNativeSeparators(m_Settings.getDownloadDirectory())) {
    if (m_DownloadManager.downloadsInProgress()) {
      MessageDialog::showMessage(tr("Can't change download directory while downloads are in progress!"), this);
    } else {
      m_DownloadManager.setOutputDirectory(m_Settings.getDownloadDirectory());
    }
  }

  if (m_Settings.getModDirectory() != oldModDirectory) {
    refreshModList();
    refreshLists();
  }

  if (m_Settings.getCacheDirectory() != oldCacheDirectory) {
    NexusInterface::instance()->setCacheDirectory(m_Settings.getCacheDirectory());
  }

  NexusInterface::instance()->setNMMVersion(m_Settings.getNMMVersion());
}

void MainWindow::on_actionNexus_triggered()
{
  QString username, password;
  m_NexusDialog.openUrl(ToQString(GameInfo::instance().getNexusPage()));

  if (m_Settings.getNexusLogin(username, password)) {
    m_NexusDialog.login(username, password);
  } else {
    m_NexusDialog.loadNexus();
  }
  m_NexusDialog.show();
  m_NexusDialog.activateWindow();

  QTabWidget *tabWidget = findChild<QTabWidget*>("tabWidget");
  tabWidget->setCurrentIndex(4);
}


void MainWindow::nexusLinkActivated(const QString &link)
{
  if (m_Settings.preferExternalBrowser()) {
    ::ShellExecuteW(NULL, L"open", ToWString(link).c_str(), NULL, NULL, SW_SHOWNORMAL);
  } else {
    QString username, password;
    m_NexusDialog.openUrl(link);
    if (m_Settings.getNexusLogin(username, password)) {
      m_NexusDialog.login(username, password);
      m_LoginAttempted = true;
    } else {
      m_NexusDialog.loadNexus();
    }
    m_NexusDialog.show();

    QTabWidget *tabWidget = findChild<QTabWidget*>("tabWidget");
    tabWidget->setCurrentIndex(4);
  }
}


void MainWindow::downloadRequestedNXM(const QString &url)
{
  QString username, password;

  if (!m_LoginAttempted && !NexusInterface::instance()->getAccessManager()->loggedIn() &&
      (m_Settings.getNexusLogin(username, password) ||
      (m_AskForNexusPW && queryLogin(username, password)))) {
    m_PendingDownloads.append(url);
    NexusInterface::instance()->getAccessManager()->login(username, password);
    m_LoginAttempted = true;
  } else {
    m_DownloadManager.addNXMDownload(url);
  }
}


void MainWindow::downloadRequested(QNetworkReply *reply, int modID, const QString &fileName)
{
  try {
    if (m_DownloadManager.addDownload(reply, fileName, modID)) {
      MessageDialog::showMessage(tr("Download started"), this);
    }
  } catch (const std::exception &e) {
    MessageDialog::showMessage(tr("Download failed"), this);
    qCritical("exception starting download: %s", e.what());
  }
}


void MainWindow::languageChange(const QString &newLanguage)
{
  if (m_Translator != NULL) {
    QCoreApplication::removeTranslator(m_Translator);
    delete m_Translator;
    m_Translator = NULL;
  }
  if (m_TranslatorQt != NULL) {
    QCoreApplication::removeTranslator(m_TranslatorQt);
    delete m_TranslatorQt;
    m_TranslatorQt = NULL;
  }

  if (newLanguage != "en_US") {
    // add our own translations
    m_Translator = new QTranslator(this);
    QString locFile = ToQString(AppConfig::translationPrefix()) + "_" + newLanguage;
    if (!m_Translator->load(locFile, QCoreApplication::applicationDirPath() + "/translations")) {
      qDebug("localization %s not found", locFile.toUtf8().constData());
    }
    QCoreApplication::installTranslator(m_Translator);

    // also add the translations for qt default strings
    m_TranslatorQt = new QTranslator(this);
    locFile = QString("qt_") + newLanguage;
    if (!m_TranslatorQt->load(locFile, QCoreApplication::applicationDirPath() + "/translations")) {
      qDebug("localization %s not found", locFile.toUtf8().constData());
    }
    QCoreApplication::installTranslator(m_TranslatorQt);
  }
  ui->retranslateUi(this);
  ui->profileBox->setItemText(0, QObject::tr("<Manage...>"));
//  ui->toolBar->addWidget(createHelpWidget(ui->toolBar));

  updateDownloadListDelegate();
  updateProblemsButton();
}


void MainWindow::installDownload(int index)
{
  try {
    QString fileName = m_DownloadManager.getFilePath(index);
    int modID = m_DownloadManager.getModID(index);
    QString modName;

    // see if there already are mods with the specified mod id
    if (modID != 0) {
      ModInfo::Ptr modInfo = ModInfo::getByModID(modID, true);
      if (!modInfo.isNull()) {
        std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
        if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_BACKUP) == flags.end()) {
          modName = modInfo->name();
          modInfo->saveMeta();
        }
      }
      // TODO there may be multiple mods with the same id!
//      modName = m_ModList.getModByModID(modID);
    }

    m_CurrentProfile->writeModlistNow();

    bool hasIniTweaks = false;
    if (m_InstallationManager.install(fileName, m_CurrentProfile->getPluginsFileName(), m_Settings.getModDirectory(), m_Settings.preferIntegratedInstallers(),
                                      m_Settings.enableQuickInstaller(), modName, hasIniTweaks)) {
      MessageDialog::showMessage(tr("Installation successful"), this);

      refreshModList();

      QModelIndexList posList = m_ModList.match(m_ModList.index(0, 0), Qt::DisplayRole, modName);
      if (posList.count() == 1) {
        ui->modList->scrollTo(posList.at(0));
      }
      int modIndex = ModInfo::getIndex(modName);
      if (modIndex != UINT_MAX) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);

        if (hasIniTweaks &&
            (QMessageBox::question(this, tr("Configure Mod"),
                tr("This mod contains ini tweaks. Do you want to configure them now?"),
                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)) {
          displayModInformation(modInfo, modIndex, ModInfoDialog::TAB_INIFILES);
        }
        testExtractBSA(modIndex);
      } else {
        reportError(tr("mod \"%1\" not found").arg(modName));
      }

      m_DownloadManager.markInstalled(index);

      emit modInstalled();
    } else if (m_InstallationManager.wasCancelled()) {
      QMessageBox::information(this, tr("Installation cancelled"), tr("The mod was not installed completely."), QMessageBox::Ok);
    }
  } catch (const std::exception &e) {
    reportError(e.what());
  }
}


void MainWindow::writeDataToFile(QFile &file, const QString &directory, const DirectoryEntry &directoryEntry)
{
  { // list files
//    std::set<FileEntry>::const_iterator current, end;
//    directoryEntry.getFiles(current, end);
//    for (; current != end; ++current) {

    std::vector<FileEntry*> files = directoryEntry.getFiles();
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      FileEntry *current = *iter;
      bool isArchive = false;
      int origin = current->getOrigin(isArchive);
      if (isArchive) {
        // TODO: don't list files from archives. maybe make this an option?
        continue;
      }
      QString fullName = directory;
      fullName.append("\\").append(ToQString(current->getName()));
      file.write(fullName.toUtf8());

      file.write("\t(");
      file.write(ToQString(m_DirectoryStructure->getOriginByID(origin).getName()).toUtf8());
      file.write(")\r\n");
    }
  }

  { // recurse into subdirectories
    std::vector<DirectoryEntry*>::const_iterator current, end;
    directoryEntry.getSubDirectories(current, end);
    for (; current != end; ++current) {
      writeDataToFile(file, directory.mid(0).append("\\").append(ToQString((*current)->getName())), **current);
    }
  }
}


void MainWindow::writeDataToFile()
{
  QString fileName = QFileDialog::getSaveFileName(this);
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly)) {
    reportError(tr("failed to write to file %1").arg(fileName));
  }

  writeDataToFile(file, "data", *m_DirectoryStructure);
  file.close();

  MessageDialog::showMessage(tr("%1 written").arg(QDir::toNativeSeparators(fileName)), this);
}


int MainWindow::getBinaryExecuteInfo(const QFileInfo &targetInfo,
                                    QFileInfo &binaryInfo, QString &arguments)
{
  QString extension = targetInfo.completeSuffix();
  if ((extension == "exe") ||
      (extension == "cmd") ||
      (extension == "com") ||
      (extension == "bat")) {
    binaryInfo = QFileInfo("C:\\Windows\\System32\\cmd.exe");
    arguments = QString("/C \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    return 1;
  } else if (extension == "jar") {
    // types that need to be injected into
    std::wstring targetPathW = ToWString(targetInfo.absoluteFilePath());
    QString binaryPath;

    { // try to find java automatically
      WCHAR buffer[MAX_PATH];
      if (::FindExecutableW(targetPathW.c_str(), NULL, buffer) > (HINSTANCE)32) {
        DWORD binaryType = 0UL;
        if (!::GetBinaryTypeW(targetPathW.c_str(), &binaryType)) {
          qDebug("failed to determine binary type: %lu", ::GetLastError());
        } else if (binaryType == SCS_32BIT_BINARY) {
          binaryPath = ToQString(buffer);
        }
      }
    }
    if (binaryPath.isEmpty() && (extension == "jar")) {
      QSettings javaReg("HKEY_LOCAL_MACHINE\\Software\\JavaSoft\\Java Runtime Environment", QSettings::NativeFormat);
      if (javaReg.contains("CurrentVersion")) {
        QString currentVersion = javaReg.value("CurrentVersion").toString();
        binaryPath = javaReg.value(QString("%1/JavaHome").arg(currentVersion)).toString().append("\\bin\\javaw.exe");
      }
    }
    if (binaryPath.isEmpty()) {
      binaryPath = QFileDialog::getOpenFileName(this, tr("Select binary"), QString(), tr("Binary") + " (*.exe)");
    }
    if (binaryPath.isEmpty()) {
      return 0;
    }
    binaryInfo = QFileInfo(binaryPath);
    if (extension == "jar") {
      arguments = QString("-jar \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    } else {
      arguments = QString("\"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
    }
    return 1;
  } else {
    return 2;
  }
}


void MainWindow::addAsExecutable()
{
  if (m_ContextItem != NULL) {
    QFileInfo targetInfo(m_ContextItem->data(0, Qt::UserRole).toString());
    QFileInfo binaryInfo;
    QString arguments;
    switch (getBinaryExecuteInfo(targetInfo, binaryInfo, arguments)) {
      case 1: {
        QString name = QInputDialog::getText(this, tr("Enter Name"),
              tr("Please enter a name for the executable"), QLineEdit::Normal,
              targetInfo.baseName());
        if (!name.isEmpty()) {
          m_ExecutablesList.addExecutable(name, binaryInfo.absoluteFilePath(),
                                          arguments, targetInfo.absolutePath(),
                                          DEFAULT_STAY, QString(),
                                          true, false);
          refreshExecutablesList();
        }
      } break;
      case 2: {
        QMessageBox::information(this, tr("Not an executable"), tr("This is not a recognized executable."));
      } break;
      default: {
        // nop
      } break;
    }
  }
}


void MainWindow::originModified(int originID)
{
  FilesOrigin &origin = m_DirectoryStructure->getOriginByID(originID);
  origin.enable(false);
  m_DirectoryStructure->addFromOrigin(origin.getName(), origin.getPath(), origin.getPriority());
  DirectoryRefresher::cleanStructure(m_DirectoryStructure);
}


void MainWindow::hideFile()
{
  QString oldName = m_ContextItem->data(0, Qt::UserRole).toString();
  QString newName = oldName + ModInfo::s_HiddenExt;

  if (QFileInfo(newName).exists()) {
    if (QMessageBox::question(this, tr("Replace file?"), tr("There already is a hidden version of this file. Replace it?"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QFile(newName).remove()) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(newName));
        return;
      }
    } else {
      return;
    }
  }

  if (QFile::rename(oldName, newName)) {
    originModified(m_ContextItem->data(1, Qt::UserRole + 1).toInt());
    refreshDataTree();
  } else {
    reportError(tr("failed to rename \"%1\" to \"%2\"").arg(oldName).arg(QDir::toNativeSeparators(newName)));
  }
}


void MainWindow::unhideFile()
{
  QString oldName = m_ContextItem->data(0, Qt::UserRole).toString();
  QString newName = oldName.left(oldName.length() - ModInfo::s_HiddenExt.length());
  if (QFileInfo(newName).exists()) {
    if (QMessageBox::question(this, tr("Replace file?"), tr("There already is a visible version of this file. Replace it?"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QFile(newName).remove()) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(newName));
        return;
      }
    } else {
      return;
    }
  }
  if (QFile::rename(oldName, newName)) {
    originModified(m_ContextItem->data(1, Qt::UserRole + 1).toInt());
    refreshDataTree();
  } else {
    reportError(tr("failed to rename \"%1\" to \"%2\"").arg(QDir::toNativeSeparators(oldName)).arg(QDir::toNativeSeparators(newName)));
  }
}


void MainWindow::openDataFile()
{
  if (m_ContextItem != NULL) {
    QFileInfo targetInfo(m_ContextItem->data(0, Qt::UserRole).toString());
    QFileInfo binaryInfo;
    QString arguments;
    switch (getBinaryExecuteInfo(targetInfo, binaryInfo, arguments)) {
      case 1: {
        spawnBinaryDirect(binaryInfo, arguments, m_CurrentProfile->getName(), targetInfo.absolutePath(), "");
      } break;
      case 2: {
        ::ShellExecuteW(NULL, L"open", ToWString(targetInfo.absoluteFilePath()).c_str(), NULL, NULL, SW_SHOWNORMAL);
      } break;
      default: {
        // nop
      } break;
    }
  }
}


void MainWindow::updateAvailable()
{
  QToolBar *toolBar = findChild<QToolBar*>("toolBar");
  foreach (QAction *action, toolBar->actions()) {
    if (action->text() == tr("Update")) {
      action->setEnabled(true);
      action->setToolTip(tr("Update available"));
      break;
    }
  }
}


void MainWindow::motdReceived(const QString &motd)
{
  // don't show motd after 5 seconds, may be annoying. Hopefully the user's
  // internet connection is faster next time
  if (m_StartTime.secsTo(QTime::currentTime()) < 5) {
    uint hash = qHash(motd);
    if (hash != m_Settings.getMotDHash()) {
      MotDDialog dialog(motd);
      dialog.exec();
      m_Settings.setMotDHash(hash);
    }
  }

  ui->actionEndorseMO->setVisible(false);
}


void MainWindow::notEndorsedYet()
{
  ui->actionEndorseMO->setVisible(true);
}


void MainWindow::on_dataTree_customContextMenuRequested(const QPoint &pos)
{
  QTreeWidget *dataTree = findChild<QTreeWidget*>("dataTree");
  m_ContextItem = dataTree->itemAt(pos.x(), pos.y());

  QMenu menu;
  if ((m_ContextItem != NULL) && (m_ContextItem->childCount() == 0)) {
    menu.addAction(tr("Open/Execute"), this, SLOT(openDataFile()));
    menu.addAction(tr("Add as Executable"), this, SLOT(addAsExecutable()));
    // offer to hide/unhide file, but not for files from archives
    if (!m_ContextItem->data(0, Qt::UserRole + 1).toBool()) {
      if (m_ContextItem->text(0).endsWith(ModInfo::s_HiddenExt)) {
        menu.addAction(tr("Un-Hide"), this, SLOT(unhideFile()));
      } else {
        menu.addAction(tr("Hide"), this, SLOT(hideFile()));
      }
    }
    menu.addSeparator();
  }
  menu.addAction(tr("Write To File..."), this, SLOT(writeDataToFile()));
  menu.addAction(tr("Refresh"), this, SLOT(on_btnRefreshData_clicked()));

  menu.exec(dataTree->mapToGlobal(pos));
}

void MainWindow::on_conflictsCheckBox_toggled(bool)
{
  refreshDataTree();
}


void MainWindow::on_actionUpdate_triggered()
{
  QString username, password;

  if (!m_LoginAttempted && !NexusInterface::instance()->getAccessManager()->loggedIn() &&
      (m_Settings.getNexusLogin(username, password) ||
      (m_AskForNexusPW && queryLogin(username, password)))) {
    NexusInterface::instance()->getAccessManager()->login(username, password);
    m_LoginAttempted = true;
  } else {
    m_Updater.startUpdate();
  }
}


void MainWindow::on_actionEndorseMO_triggered()
{
  if (QMessageBox::question(this, tr("Endorse Mod Organizer"),
                            tr("Do you want to endorse Mod Organizer on %1 now?").arg(ToQString(GameInfo::instance().getNexusPage())),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    NexusInterface::instance()->requestToggleEndorsement(GameInfo::instance().getNexusModID(), true, this, QVariant());
  }
}


void MainWindow::updateDownloadListDelegate()
{
  if (ui->compactBox->isChecked()) {
    ui->downloadView->setItemDelegate(new DownloadListWidgetCompactDelegate(&m_DownloadManager, ui->downloadView));
  } else {
    ui->downloadView->setItemDelegate(new DownloadListWidgetDelegate(&m_DownloadManager, ui->downloadView));
  }

  DownloadListSortProxy *sortProxy = new DownloadListSortProxy(&m_DownloadManager, ui->downloadView);
  sortProxy->setSourceModel(new DownloadList(&m_DownloadManager, ui->downloadView));
  connect(ui->downloadFilterEdit, SIGNAL(textChanged(QString)), sortProxy, SLOT(updateFilter(QString)));

  ui->downloadView->setModel(sortProxy);
  ui->downloadView->sortByColumn(1, Qt::AscendingOrder);
  ui->downloadView->header()->resizeSections(QHeaderView::Fixed);
//  ui->downloadView->setFirstColumnSpanned(0, QModelIndex(), true);

  connect(ui->downloadView->itemDelegate(), SIGNAL(installDownload(int)), this, SLOT(installDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(queryInfo(int)), &m_DownloadManager, SLOT(queryInfo(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(removeDownload(int, bool)), &m_DownloadManager, SLOT(removeDownload(int, bool)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(cancelDownload(int)), &m_DownloadManager, SLOT(cancelDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(pauseDownload(int)), &m_DownloadManager, SLOT(pauseDownload(int)));
  connect(ui->downloadView->itemDelegate(), SIGNAL(resumeDownload(int)), &m_DownloadManager, SLOT(resumeDownload(int)));
}


void MainWindow::on_compactBox_toggled(bool)
{
  updateDownloadListDelegate();
}


void MainWindow::modDetailsUpdated(bool)
{
  --m_ModsToUpdate;
  if (m_ModsToUpdate == 0) {
    statusBar()->hide();
    m_ModListSortProxy->setCategoryFilter(CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE);
    for (int i = 0; i < ui->categoriesList->topLevelItemCount(); ++i) {
      if (ui->categoriesList->topLevelItem(i)->data(0, Qt::UserRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
        ui->categoriesList->setCurrentItem(ui->categoriesList->topLevelItem(i));
        break;
      }
    }
//    m_RefreshProgress->setVisible(false);
  } else {
    m_RefreshProgress->setValue(m_RefreshProgress->maximum() - m_ModsToUpdate);
  }
}

void MainWindow::nxmUpdatesAvailable(const std::vector<int> &modIDs, QVariant userData, QVariant resultData, int)
{
  m_ModsToUpdate -= modIDs.size();

  QVariantList resultList = resultData.toList();
  for (auto iter = resultList.begin(); iter != resultList.end(); ++iter) {
    QVariantMap result = iter->toMap();
    if (result["id"].toInt() == GameInfo::instance().getNexusModID()) {
      if (!result["voted_by_user"].toBool()) {
        ui->actionEndorseMO->setVisible(true);
      }
    } else {
      ModInfo::Ptr info = ModInfo::getByModID(result["id"].toInt(), true);
      if (!info.isNull()) {
        info->setNewestVersion(VersionInfo(result["version"].toString()));
        info->setNexusDescription(result["description"].toString());
        if (NexusInterface::instance()->getAccessManager()->loggedIn()) {
          // don't use endorsement info if we're not logged in
          info->setIsEndorsed(result["voted_by_user"].toBool());
        }
      }
    }
  }

  if (m_ModsToUpdate <= 0) {
    statusBar()->hide();
    m_ModListSortProxy->setCategoryFilter(CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE);
    for (int i = 0; i < ui->categoriesList->topLevelItemCount(); ++i) {
      if (ui->categoriesList->topLevelItem(i)->data(0, Qt::UserRole) == CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE) {
        ui->categoriesList->setCurrentItem(ui->categoriesList->topLevelItem(i));
        break;
      }
    }
  } else {
    m_RefreshProgress->setValue(m_RefreshProgress->maximum() - m_ModsToUpdate);
  }
}


void MainWindow::nxmEndorsementToggled(int, QVariant, QVariant resultData, int)
{
  if (resultData.toBool()) {
    ui->actionEndorseMO->setVisible(false);
    QMessageBox::question(this, tr("Thank you!"), tr("Thank you for your endorsement!"));
  }

  if (!disconnect(sender(), SIGNAL(nxmEndorsementToggled(int, QVariant, QVariant, int)),
             this, SLOT(nxmEndorsementToggled(int, QVariant, QVariant, int)))) {
    qCritical("failed to disconnect endorsement slot");
  }
}


void MainWindow::nxmRequestFailed(int modID, QVariant, int, const QString &errorString)
{
  if (modID == -1) {
    // must be the update-check that failed
    m_ModsToUpdate = 0;
    statusBar()->hide();
  }
  MessageDialog::showMessage(tr("Request to Nexus failed: %1").arg(errorString), this);
}


void MainWindow::loginSuccessful(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), this);
  }
  foreach (QString url, m_PendingDownloads) {
    downloadRequestedNXM(url);
  }
  m_PendingDownloads.clear();
  foreach (auto task, m_PostLoginTasks) {
    task(this);
  }

  m_PostLoginTasks.clear();
}


void MainWindow::loginSuccessfulUpdate(bool necessary)
{
  if (necessary) {
    MessageDialog::showMessage(tr("login successful"), this);
  }
  m_Updater.startUpdate();
}


void MainWindow::loginFailed(const QString &message)
{
  if (!m_PendingDownloads.isEmpty()) {
    MessageDialog::showMessage(tr("login failed: %1. Trying to download anyway").arg(message), this);
    foreach (QString url, m_PendingDownloads) {
      downloadRequestedNXM(url);
    }
    m_PendingDownloads.clear();
  } else {
    MessageDialog::showMessage(tr("login failed: %1").arg(message), this);
    m_PostLoginTasks.clear();
    statusBar()->hide();
  }
}


void MainWindow::loginFailedUpdate(const QString &message)
{
  MessageDialog::showMessage(tr("login failed: %1. You need to log-in with Nexus to update MO.").arg(message), this);
}


void MainWindow::windowTutorialFinished(const QString &windowName)
{
  m_Settings.directInterface().setValue(QString("CompletedWindowTutorials/") + windowName, true);
}


BSA::EErrorCode MainWindow::extractBSA(BSA::Archive &archive, BSA::Folder::Ptr folder, const QString &destination,
                           QProgressDialog &progress)
{
  QDir().mkdir(destination);
  BSA::EErrorCode result = BSA::ERROR_NONE;
  QString errorFile;

  for (unsigned int i = 0; i < folder->getNumFiles(); ++i) {
    BSA::File::Ptr file = folder->getFile(i);
    BSA::EErrorCode res = archive.extract(file, destination.toUtf8().constData());
    if (res != BSA::ERROR_NONE) {
      reportError(tr("failed to read %1: %2").arg(file->getName().c_str()).arg(res));
      result = res;
    }
    progress.setLabelText(file->getName().c_str());
    progress.setValue(progress.value() + 1);
    QCoreApplication::processEvents();
    if (progress.wasCanceled()) {
      result = BSA::ERROR_CANCELED;
    }
  }

  if (result != BSA::ERROR_NONE) {
    if (QMessageBox::critical(this, tr("Error"), tr("failed to extract %1 (errorcode %2)").arg(errorFile).arg(result),
                              QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
      return result;
    }
  }

  for (unsigned int i = 0; i < folder->getNumSubFolders(); ++i) {
    BSA::Folder::Ptr subFolder = folder->getSubFolder(i);
    BSA::EErrorCode res = extractBSA(archive, subFolder,
                                     destination.mid(0).append("/").append(subFolder->getName().c_str()), progress);
    if (res != BSA::ERROR_NONE) {
      return res;
    }
  }
  return BSA::ERROR_NONE;
}


bool MainWindow::extractProgress(QProgressDialog &progress, int percentage, std::string fileName)
{
  progress.setLabelText(fileName.c_str());
  progress.setValue(percentage);
  QCoreApplication::processEvents();
  return !progress.wasCanceled();
}


void MainWindow::extractBSATriggered()
{
  QTreeWidgetItem *item = ui->bsaList->topLevelItem(m_ContextRow);

  QString targetFolder = FileDialogMemory::getExistingDirectory("extractBSA", this, tr("Extract BSA"));

  if (!targetFolder.isEmpty()) {
    BSA::Archive archive;
    QString originPath = QDir::fromNativeSeparators(ToQString(m_DirectoryStructure->getOriginByName(ToWString(item->text(1))).getPath()));
    QString archivePath =  QString("%1\\%2").arg(originPath).arg(item->text(0));

    BSA::EErrorCode result = archive.read(archivePath.toLocal8Bit().constData());
    if ((result != BSA::ERROR_NONE) && (result != BSA::ERROR_INVALIDHASHES)) {
      reportError(tr("failed to read %1: %2").arg(archivePath).arg(result));
      return;
    }

    QProgressDialog progress(this);
    progress.setMaximum(100);
    progress.setValue(0);
    progress.show();

    archive.extractAll(QDir::toNativeSeparators(targetFolder).toUtf8().constData(),
                       boost::bind(&MainWindow::extractProgress, this, boost::ref(progress), _1, _2));

    if (result == BSA::ERROR_INVALIDHASHES) {
      reportError(tr("This archive contains invalid hashes. Some files may be broken."));
    }
  }
}


void MainWindow::displayColumnSelection(const QPoint &pos)
{
  QMenu menu;

  // display a list of all headers as checkboxes
  QAbstractItemModel *model = ui->modList->header()->model();
  for (int i = 0; i < model->columnCount(); ++i) {
    QString columnName = model->headerData(i, Qt::Horizontal).toString();
    QCheckBox *checkBox = new QCheckBox(&menu);
    checkBox->setText(columnName);
    checkBox->setChecked(!ui->modList->header()->isSectionHidden(i));
    QWidgetAction *checkableAction = new QWidgetAction(&menu);
    checkableAction->setDefaultWidget(checkBox);
    menu.addAction(checkableAction);
  }
  menu.exec(pos);

  // view/hide columns depending on check-state
  int i = 0;
  foreach (const QAction *action, menu.actions()) {
    const QWidgetAction *widgetAction = qobject_cast<const QWidgetAction*>(action);
    if (widgetAction != NULL) {
      const QCheckBox *checkBox = qobject_cast<const QCheckBox*>(widgetAction->defaultWidget());
      if (checkBox != NULL) {
        ui->modList->header()->setSectionHidden(i, !checkBox->isChecked());
      }
    }
    ++i;
  }
}


void MainWindow::on_bsaList_customContextMenuRequested(const QPoint &pos)
{
  m_ContextRow = ui->bsaList->indexOfTopLevelItem(ui->bsaList->itemAt(pos));

  QMenu menu;
  menu.addAction(tr("Extract..."), this, SLOT(extractBSATriggered()));

  menu.exec(ui->bsaList->mapToGlobal(pos));
}

void MainWindow::on_bsaList_itemChanged(QTreeWidgetItem*, int)
{
  checkBSAList();
}

void MainWindow::on_actionProblems_triggered()
{
  QString problemDescription;
  checkForProblems(problemDescription);
  QMessageBox::information(this, tr("Problems"), problemDescription);
}

void MainWindow::setCategoryListVisible(bool visible)
{
  if (visible) {
    ui->categoriesGroup->show();
    ui->displayCategoriesBtn->setText(ToQString(L"\u00ab"));
  } else {
    ui->categoriesGroup->hide();
    ui->displayCategoriesBtn->setText(ToQString(L"\u00bb"));
  }
}

void MainWindow::on_displayCategoriesBtn_toggled(bool checked)
{
  setCategoryListVisible(checked);
}

void MainWindow::editCategories()
{
  CategoriesDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}

void MainWindow::on_categoriesList_customContextMenuRequested(const QPoint &pos)
{
  QMenu menu;
  menu.addAction(tr("Edit Categories..."), this, SLOT(editCategories()));

  menu.exec(ui->categoriesList->mapToGlobal(pos));
}


void MainWindow::lockESPIndex()
{
  m_PluginList.lockESPIndex(m_ContextRow, true);
}

void MainWindow::unlockESPIndex()
{
  m_PluginList.lockESPIndex(m_ContextRow, false);
}


void MainWindow::on_espList_customContextMenuRequested(const QPoint &pos)
{
  m_ContextRow = m_PluginListSortProxy->mapToSource(ui->espList->indexAt(pos)).row();

  QMenu menu;
  menu.addAction(tr("Enable all"), &m_PluginList, SLOT(enableAll()));
  menu.addAction(tr("Disable all"), &m_PluginList, SLOT(disableAll()));

  if ((m_ContextRow != -1) && m_PluginList.isEnabled(m_ContextRow)) {
    if (m_PluginList.isESPLocked(m_ContextRow)) {
      menu.addAction(tr("Unlock index"), this, SLOT(unlockESPIndex()));
    } else {
      menu.addAction(tr("Lock index"), this, SLOT(lockESPIndex()));
    }
  }

  try {
    menu.exec(ui->espList->mapToGlobal(pos));
  } catch (const std::exception &e) {
    reportError(tr("Exception: ").arg(e.what()));
  } catch (...) {
    reportError(tr("Unknown exception"));
  }
}
