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

#include "credentialsdialog.h"
#include "ui_credentialsdialog.h"

CredentialsDialog::CredentialsDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::CredentialsDialog)
{
  ui->setupUi(this);
}

CredentialsDialog::~CredentialsDialog()
{
  delete ui;
}

QString CredentialsDialog::username() const
{
  return ui->usernameEdit->text();
}

QString CredentialsDialog::password() const
{
  return ui->passwordEdit->text();
}

bool CredentialsDialog::store() const
{
  return ui->rememberCheck->isChecked();
}

bool CredentialsDialog::neverAsk() const
{
  return ui->dontaskBox->isChecked();
}
