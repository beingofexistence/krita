/* This file is part of the KDE project
 * Copyright (C) 2007 Thomas Zander <zander@kde.org>
 * Copyright (C) 2008 Fredy Yanardi <fyanardi@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "KoFind.h"
#include "KoText.h"

#include <KoCanvasResourceProvider.h>

#include <KActionCollection>
#include <KWindowSystem>
#include <KFindDialog>
#include <KReplaceDialog>
#include <KFind>
#include <KLocale>
#include <KMessageBox>
#include <KAction>

#include <QAction>
#include <QTextDocument>
#include <QTextCursor>
#include <QTimer>

#include <KDebug>

class NonClosingFindDialog : public KFindDialog {
public:
    NonClosingFindDialog(QWidget *parent) : KFindDialog(parent) {}

    virtual void accept() {}
};

class KoFind::Private {
public:
    Private(KoFind *find, KoCanvasResourceProvider *crp, QWidget *w)
        :provider(crp),
        widget(w),
        findDialog(0),
        replaceDialog(0),
        parent(find),
        findNext(0),
        findPrev(0),
        document(0),
        startedPosition(-1),
        matches(0),
        replaced(0),
        restarted(false),
        lastDialogUsed(0)
    {
    }

    void resourceChanged(int key, const QVariant &variant) {
        if(key == KoText::CurrentTextDocument) {
            document = static_cast<QTextDocument*> (variant.value<void*>());
            lastKnownPosition = QTextCursor(); // make invalid
        }
        else if(findDialog && (key == KoText::CurrentTextPosition || key == KoText::CurrentTextAnchor)) {
            const int selectionStart = provider->intResource(KoText::CurrentTextPosition);
            const int selectionEnd = provider->intResource(KoText::CurrentTextAnchor);
            findDialog->setHasSelection(selectionEnd != selectionStart);
        }
    }
    void findActivated() {
        lastKnownPosition = QTextCursor();
        if(findDialog) {
            findDialog->show();
            KWindowSystem::activateWindow( findDialog->winId() );
            return;
        }
        findDialog = new NonClosingFindDialog(widget);
        findDialog->setOptions(KFind::FromCursor);
        const int selectionStart = provider->intResource(KoText::CurrentTextPosition);
        const int selectionEnd = provider->intResource(KoText::CurrentTextAnchor);
        findDialog->setHasSelection(selectionEnd != selectionStart);
        connect( findDialog, SIGNAL(okClicked()), parent, SLOT(startFind()) );
        findDialog->show();

        findNext->setEnabled(true);
        findPrev->setEnabled(true);

    }
    void findNextActivated() {
        Q_ASSERT(findDialog);
        findDialog->setOptions( (findDialog->options() | KFind::FindBackwards) ^ KFind::FindBackwards);
        parseSettingsAndFind(false);
    }
    void findPreviousActivated() {
        Q_ASSERT(findDialog);
        findDialog->setOptions( findDialog->options() | KFind::FindBackwards);
        parseSettingsAndFind(false);
    }
    void replaceActivated() {
        lastDialogUsed = 1;
        if (replaceDialog) {
            replaceDialog->show();
            KWindowSystem::activateWindow( replaceDialog->winId() );
            if (findDialog && lastDialogUsed == 0) {
                replaceDialog->setOptions(findDialog->options()); // copy options from find dialog
                replaceDialog->setFindHistory(findDialog->findHistory());
            }
            return;
        }
        replaceDialog = new KReplaceDialog(widget);
        if (findDialog)
            replaceDialog->setOptions(findDialog->options()); // copy options from find dialog
        else {
            replaceDialog->setOptions(KFind::FromCursor);
            const int selectionStart = provider->intResource(KoText::CurrentTextPosition);
            const int selectionEnd = provider->intResource(KoText::CurrentTextAnchor);
            replaceDialog->setHasSelection(selectionEnd != selectionStart);
            connect( replaceDialog, SIGNAL(okClicked()), parent, SLOT(startReplace()) );
        }
        replaceDialog->show();

        findNext->setEnabled(true);
        findPrev->setEnabled(true);
    }

    void startFind() { // executed when the user presses the 'find' button.
        parseSettingsAndFind(false);

        QTimer::singleShot(0, findDialog, SLOT(show())); // show the findDialog again.
    }

    void startReplace() {
        parseSettingsAndFind(true);

        // QTimer::singleShot(0, replaceDialog, SLOT(show()));
    }

    void parseSettingsAndFind(bool replace) {
        if(document == 0)
            return;
        long options;
        if (replace) {
            replaceDialog->hide(); // We don't want the replace dialog to keep popping up
            options = replaceDialog->options();
        }
        else
            options = findDialog->options();

        QTextDocument::FindFlags flags;
        if((options & KFind::WholeWordsOnly) != 0)
            flags |= QTextDocument::FindWholeWords;
        if((options & KFind::CaseSensitive) != 0)
            flags |= QTextDocument::FindCaseSensitively;
        if((options & KFind::FindBackwards) != 0)
            flags |= QTextDocument::FindBackward;
        if(lastKnownPosition.isNull()) {
            lastKnownPosition = QTextCursor(document);
            if((options & KFind::FromCursor) != 0)
                lastKnownPosition.setPosition(provider->intResource(KoText::CurrentTextPosition));
            startedPosition = lastKnownPosition.position();
            restarted = false;
            matches = 0;
        }
        const bool selectedText = (options & KFind::SelectedText) != 0;
        int selectionStart=0;
        int selectionEnd=0;
        if(selectedText) {
            selectionStart = provider->intResource(KoText::CurrentTextPosition);
            selectionEnd = provider->intResource(KoText::CurrentTextAnchor);
            if(selectionEnd < selectionStart)
                qSwap(selectionStart, selectionEnd);

            if(lastKnownPosition.position() < selectionStart || lastKnownPosition.position() > selectionEnd) {
                lastKnownPosition.setPosition(selectionStart);
                startedPosition = selectionStart;
            }
        }

        QTextCursor cursor;
        QRegExp regExp;
        if(options & KFind::RegularExpression)
            regExp = QRegExp(replace ? replaceDialog->pattern() : findDialog->pattern());
        if(!regExp.isEmpty() && regExp.isValid())
            cursor = document->find(regExp, lastKnownPosition, flags);
        else
            cursor = document->find(replace ? replaceDialog->pattern() : findDialog->pattern(), lastKnownPosition, flags);

        // do the replacement
        if (replace && !cursor.isNull() && cursor.selectionEnd() > cursor.selectionStart()) {
            replaced++;
            if ((options && KReplaceDialog::PromptOnReplace) != 0) {
                provider->setResource(KoText::CurrentTextPosition, cursor.position());
                provider->setResource(KoText::CurrentTextAnchor, cursor.anchor());
                provider->clearResource(KoText::SelectedTextPosition);
                provider->clearResource(KoText::SelectedTextAnchor);
                
                int value = KMessageBox::questionYesNo(widget,
                        i18n("Replace %1 with %2?", replaceDialog->pattern(), replaceDialog->replacement()));
                if (value == KMessageBox::Yes)
                    cursor.insertText(replaceDialog->replacement());
            }
            else
                cursor.insertText(replaceDialog->replacement());
            lastKnownPosition = cursor;

            if (!(restarted && cursor.position() > startedPosition || cursor.position() == -1))
                parseSettingsAndFind(replace);
        }

        if((selectedText && cursor.position() > selectionEnd || // end of selection
                cursor.position() == -1 && !restarted) && startedPosition <= lastKnownPosition.position()) { // end of doc
            // restart
            lastKnownPosition.setPosition(0);
            restarted = true;
            parseSettingsAndFind(replace);
            return;
        }

        if(restarted && cursor.position() > startedPosition || cursor.position() == -1) { // looped round.
            matches = 0;
            if (replace) {
                if (replaced == 0)
                    KMessageBox::information(widget, i18n("Found no match\n\nNo text was replaced"));
                else {
                    int value = KMessageBox::questionYesNo(widget,
                            i18np("1 replacement made\n\nDo you want to restart search from the beginning?",
                                  "%1 replacements made\n\nDo you want to restart search from the beginning?", replaced));
                    if (value == KMessageBox::Cancel) {
                        restarted = false;
                        return;
                    }
                    replaced = 0;
                }
            }
            else
                KMessageBox::information(findDialog, matches?i18np("Found 1 match", "Found %1 matches", matches):i18n("Found no match"));
            restarted = false; // allow to restart again.
            return;
        }

        if(selectedText) {
            provider->setResource(KoText::SelectedTextPosition, cursor.position());
            provider->setResource(KoText::SelectedTextAnchor, cursor.anchor());
        }
        else {
            provider->setResource(KoText::CurrentTextPosition, cursor.position());
            provider->setResource(KoText::CurrentTextAnchor, cursor.anchor());
            provider->clearResource(KoText::SelectedTextPosition);
            provider->clearResource(KoText::SelectedTextAnchor);
        }
        lastKnownPosition = cursor;
        matches++;
    }

    KoCanvasResourceProvider *provider;
    QWidget *widget;
    KFindDialog *findDialog;
    KReplaceDialog *replaceDialog;
    KoFind *parent;
    QAction *findNext, *findPrev;

    QTextDocument *document;
    int startedPosition, matches, replaced;
    QTextCursor lastKnownPosition;
    bool restarted;
    int lastDialogUsed;
};

KoFind::KoFind(QWidget *parent, KoCanvasResourceProvider *provider, KActionCollection *ac)
    : QObject(parent),
    d (new Private(this, provider, parent))
{
    connect(provider, SIGNAL(resourceChanged(int,const QVariant&)), this, SLOT(resourceChanged(int,const QVariant& )));
    ac->addAction(KStandardAction::Find, "edit_find", this, SLOT( findActivated() ));
    d->findNext = ac->addAction(KStandardAction::FindNext, "edit_findnext", this, SLOT( findNextActivated() ));
    d->findNext->setEnabled(false);
    d->findPrev = ac->addAction(KStandardAction::FindPrev, "edit_findprevious", this, SLOT( findPreviousActivated() ));
    d->findPrev->setEnabled(false);
    ac->addAction(KStandardAction::Replace, "edit_replace", this, SLOT( replaceActivated() ));
}

KoFind::~KoFind() {
    delete d;
}

#include <KoFind.moc>
