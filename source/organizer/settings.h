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

#ifndef WORKAROUNDS_H
#define WORKAROUNDS_H

#include "loadmechanism.h"
#include "serverinfo.h"
#include <iplugin.h>

#include <QSettings>
#include <QListWidget>
#include <QComboBox>


/**
 * manages the settings for Mod Organizer. The settings are not cached
 * inside the class but read/written directly from/to disc
 **/
class Settings : public QObject
{

  Q_OBJECT

public:

  /**
   * @brief constructor
   **/
  Settings();

  virtual ~Settings();

  static Settings &instance();

  /**
   * unregister all plugins from settings
   */
  void clearPlugins();

  /**
   * @brief register plugin to be configurable
   * @param plugin the plugin to register
   * @return true if the plugin may be registered, false if it is blacklisted
   */
  void registerPlugin(MOBase::IPlugin *plugin);

  /**
   * displays a SettingsDialog that allows the user to change settings. If the
   * user accepts the changes, the settings are immediately written
   **/
  void query(QWidget *parent);

  /**
   * set up the settings for the specified plugins
   **/
  void addPluginSettings(const std::vector<MOBase::IPlugin*> &plugins);

  /**
   * @return true if the user wants unchecked plugins (esp, esm) should be hidden from
   *         the virtual dat adirectory
   **/
  bool hideUncheckedPlugins() const;

  /**
   * @return true if files of the core game are forced-enabled so the user can't accidentally disable them
   */
  bool forceEnableCoreFiles() const;

  /**
   * @brief register download speed
   * @param url complete download url
   * @param bytesPerSecond download size in bytes per second
   */
  void setDownloadSpeed(const QString &serverName, int bytesPerSecond);

  /**
   * the steam appid is assigned by the steam platform to each product sold there.
   * The appid may differ between different versions of a game so it may be impossible
   * for Mod Organizer to automatically recognize it, though usually it does
   * @return the steam appid for the game
   **/
  QString getSteamAppID() const;

  /**
   * retrieve the directory where downloads are stored (with native separators)
   **/
  QString getDownloadDirectory() const;

  /**
   * retrieve a sorted list of preferred servers
   */
  std::map<QString, int> getPreferredServers();

  /**
   * retrieve the directory where mods are stored (with native separators)
   **/
  QString getModDirectory() const;

  /**
   * returns the version of nmm to impersonate when connecting to nexus
   **/
  QString getNMMVersion() const;

  /**
   * retrieve the directory where the web cache is stored (with native separators)
   **/
  QString getCacheDirectory() const;

  /**
   * @return true if the user has set up automatic login to nexus
   **/
  bool automaticLoginEnabled() const;

  /**
   * @brief retrieve the login information for nexus
   *
   * @param username (out) receives the user name for nexus
   * @param password (out) received the password for nexus
   * @return true if automatic login is active, false otherwise
   **/
  bool getNexusLogin(QString &username, QString &password) const;

  /**
   * @return true if the user disabled internet features
   */
  bool offlineMode() const;

  /**
   * @return true if the user chose compact downloads
   */
  bool compactDownloads() const;

  /**
   * @return true if the user chose meta downloads
   */
  bool metaDownloads() const;

  /**
   * @return the configured log level
   */
  int logLevel() const;

  /**
   * @brief set the nexus login information
   *
   * @param username username
   * @param password password
   */
  void setNexusLogin(QString username, QString password);

  /**
   * @return the load mechanism to be used
   **/
  LoadMechanism::EMechanism getLoadMechanism() const;

  /**
   * @brief activate the load mechanism selected by the user
   **/
  void setupLoadMechanism();

  /**
   * @return true if the user configured the use of a network proxy
   */
  bool useProxy();

  /**
   * @return true if the user wants to see non-official plugins installed outside MO in his mod list
   */
  bool displayForeign();

  /**
   * @brief sets the new motd hash
   **/
  void setMotDHash(uint hash);

  /**
   * @return hash of the last displayed message of the day
   **/
  uint getMotDHash() const;

  /**
   * @brief allows direct access to the wrapped QSettings object
   * @return the wrapped QSettings object
   */
  QSettings &directInterface() { return m_Settings; }

  /**
   * @brief retrieve a setting for one of the installed plugins
   * @param pluginName name of the plugin
   * @param key name of the setting to retrieve
   * @return the requested value as a QVariant
   * @note an invalid QVariant is returned if the the plugin/setting is not declared
   */
  QVariant pluginSetting(const QString &pluginName, const QString &key) const;

  /**
   * @brief set a setting for one of the installed mods
   * @param pluginName name of the plugin
   * @param key name of the setting to change
   * @param value the new value to set
   * @throw an exception is thrown if pluginName is invalid
   */
  void setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value);

  /**
   * @brief retrieve a persistent value for a plugin
   * @param pluginName name of the plugin to store data for
   * @param key id of the value to retrieve
   * @param def default value to return if the value is not set
   * @return the requested value
   */
  QVariant pluginPersistent(const QString &pluginName, const QString &key, const QVariant &def) const;

  /**
   * @brief set a persistent value for a plugin
   * @param pluginName name of the plugin to store data for
   * @param key id of the value to retrieve
   * @param value value to set
   * @throw an exception is thrown if pluginName is invalid
   */
  void setPluginPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync);

  /**
   * @return short code of the configured language (corresponding to the translation files)
   */
  QString language();

  /**
   * @brief updates the list of known servers
   * @param list of servers from a recent query
   */
  void updateServers(const QList<ServerInfo> &servers);

  /**
   * @brief add a plugin that is to be blacklisted
   * @param fileName name of the plugin to blacklist
   */
  void addBlacklistPlugin(const QString &fileName);

  /**
   * @brief test if a plugin is blacklisted and shouldn't be loaded
   * @param fileName name of the plugin
   * @return true if the file is blacklisted
   */
  bool pluginBlacklisted(const QString &fileName) const;

  /**
   * @return all loaded MO plugins
   */
  std::vector<MOBase::IPlugin*> plugins() const { return m_Plugins; }

  /**
   * @brief register MO as the handler for nxm links
   * @param force set to true to enforce the registration dialog to show up,
   *              even if the user said earlier not to
   */
  void registerAsNXMHandler(bool force);

private:

  QString obfuscate(const QString &password) const;
  QString deObfuscate(const QString &password) const;

  void addLanguages(QComboBox *languageBox);
  void addStyles(QComboBox *styleBox);
  bool isNXMHandler(bool *modifyable);
  void setNXMHandlerActive(bool active, bool writable);
  void readPluginBlacklist();
  void writePluginBlacklist();

private slots:

  void resetDialogs();

signals:

  void languageChanged(const QString &newLanguage);
  void styleChanged(const QString &newStyle);

private:

  static Settings *s_Instance;

  QSettings m_Settings;

  LoadMechanism m_LoadMechanism;

  std::vector<MOBase::IPlugin*> m_Plugins;

  QMap<QString, QMap<QString, QVariant> > m_PluginSettings;

  QSet<QString> m_PluginBlacklist;

};

#endif // WORKAROUNDS_H
