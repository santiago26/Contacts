#include "emoticons.h"

#include <QSet>
#include <QTextBlock>

#define DEFAULT_ICONSET                 "smiles"

#ifdef Q_WS_MAC
template <typename T>
QList<T> reversed(const QList<T> & in)
{
	QList<T> result;
	result.reserve(in.size());
	std::reverse_copy(in.begin(), in.end(), std::back_inserter(result));
	return result;
}
#endif

Emoticons::Emoticons()
{
	FMessageWidgets = NULL;
	FMessageProcessor = NULL;
	FOptionsManager = NULL;
#ifdef Q_WS_MAC
	FMacIntegration = NULL;
#endif
}

Emoticons::~Emoticons()
{
	clearTreeItem(&FRootTreeItem);
}

void Emoticons::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Emoticons");
	APluginInfo->description = tr("Allows to use your smiley images in messages");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Potapov S.A. aka Lion";
	APluginInfo->homePage = "http://contacts.rambler.ru";
	APluginInfo->dependences.append(MESSAGEWIDGETS_UUID);
}

bool Emoticons::initConnections(IPluginManager *APluginManager, int &AInitOrder)
{
	Q_UNUSED(AInitOrder);
	IPlugin *plugin = APluginManager->pluginInterface("IMessageProcessor").value(0,NULL);
	if (plugin)
	{
		FMessageProcessor = qobject_cast<IMessageProcessor *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IMessageWidgets").value(0,NULL);
	if (plugin)
	{
		FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
		if (FMessageWidgets)
		{
			connect(FMessageWidgets->instance(),SIGNAL(editWidgetCreated(IEditWidget *)),SLOT(onEditWidgetCreated(IEditWidget *)));
		}
	}

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
	{
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());
	}

#ifdef Q_WS_MAC
	plugin = APluginManager->pluginInterface("IMacIntegration").value(0,NULL);
	if (plugin)
	{
		FMacIntegration = qobject_cast<IMacIntegration *>(plugin->instance());
	}
#endif

	connect(Options::instance(),SIGNAL(optionsOpened()),SLOT(onOptionsOpened()));
	connect(Options::instance(),SIGNAL(optionsChanged(const OptionsNode &)),SLOT(onOptionsChanged(const OptionsNode &)));

	return FMessageWidgets!=NULL;
}

bool Emoticons::initObjects()
{
#ifdef Q_WS_MAC
	emoticonsMenu = new Menu;
	emoticonsMenu->menuAction()->setText(tr("Insert Emoticon"));
	emoticonsMenu->setEnabled(false);
	if (FMacIntegration)
	{
		FMacIntegration->editMenu()->addAction(emoticonsMenu->menuAction(), 700);
	}
#endif
	return true;
}

bool Emoticons::initSettings()
{
	Options::setDefaultValue(OPV_MESSAGES_EMOTICONS,QStringList() << DEFAULT_ICONSET);
	Options::setDefaultValue(OPV_MESSAGES_EMOTICONS_ENABLED, true);

	if (FOptionsManager)
	{
		FOptionsManager->insertServerOption(OPV_MESSAGES_EMOTICONS_ENABLED);
		FOptionsManager->insertOptionsHolder(this);
	}
	return true;
}

QMultiMap<int, IOptionsWidget *> Emoticons::optionsWidgets(const QString &ANodeId, QWidget *AParent)
{
	QMultiMap<int, IOptionsWidget *> widgets;
	if (FOptionsManager && ANodeId == OPN_MESSAGES)
	{
		widgets.insertMulti(OWO_MESSAGES_EMOTICONS, FOptionsManager->optionsHeaderWidget(QString::null,tr("Smiley usage in messages"),AParent));
		widgets.insertMulti(OWO_MESSAGES_EMOTICONS, FOptionsManager->optionsNodeWidget(Options::node(OPV_MESSAGES_EMOTICONS_ENABLED), tr("Automatically convert text smiles to graphical"),AParent));
	}
	return widgets;
}

void Emoticons::writeTextToMessage(int AOrder, Message &AMessage, QTextDocument *ADocument, const QString &ALang)
{
	Q_UNUSED(AMessage);	Q_UNUSED(ALang);
	if (AOrder == MWO_EMOTICONS)
		replaceImageToText(ADocument);
}

void Emoticons::writeMessageToText(int AOrder, Message &AMessage, QTextDocument *ADocument, const QString &ALang)
{
	Q_UNUSED(AMessage);	Q_UNUSED(ALang);
	if (AOrder == MWO_EMOTICONS)
		replaceTextToImage(ADocument);
}

QList<QString> Emoticons::activeIconsets() const
{
	QList<QString> iconsets = Options::node(OPV_MESSAGES_EMOTICONS).value().toStringList();
	for (QList<QString>::iterator it = iconsets.begin(); it != iconsets.end(); )
	{
		if (!FStorages.contains(*it))
			it = iconsets.erase(it);
		else
			it++;
	}
	return iconsets;
}

QUrl Emoticons::urlByKey(const QString &AKey) const
{
	return FUrlByKey.value(AKey);
}

QString Emoticons::keyByUrl(const QUrl &AUrl) const
{
	return FKeyByUrl.value(AUrl.toString());
}

QMap<int, QString> Emoticons::findTextEmoticons(const QTextDocument *ADocument, int AStartPos, int ALength) const
{
	QMap<int,QString> emoticons;
	QTextBlock block = ADocument->findBlock(AStartPos);
	int stopPos = ALength < 0 ? ADocument->characterCount() : AStartPos+ALength;
	while (block.isValid() && block.position()<stopPos)
	{
		for (QTextBlock::iterator it = block.begin(); !it.atEnd(); it++)
		{
			QTextFragment fragment = it.fragment();
			if (fragment.length()>0 && fragment.position()<stopPos)
			{
				bool searchStarted = true;
				QString searchText = fragment.text();
				for (int keyPos=0; keyPos<searchText.length(); keyPos++)
				{
					searchStarted = searchStarted || searchText.at(keyPos).isSpace();
					if (searchStarted && !searchText.at(keyPos).isSpace())
					{
						int keyLength = 0;
						const EmoticonTreeItem *item = &FRootTreeItem;
						while (item && keyLength<=searchText.length()-keyPos && fragment.position()+keyPos+keyLength<=stopPos)
						{
							const QChar nextChar = keyPos+keyLength<searchText.length() ? searchText.at(keyPos+keyLength) : QChar(' ');
							if (!item->url.isEmpty() && nextChar.isSpace())
							{
								emoticons.insert(fragment.position()+keyPos,searchText.mid(keyPos,keyLength));
								keyPos += keyLength-1;
								item = NULL;
							}
							else
							{
								keyLength++;
								item = item->childs.value(nextChar);
							}
						}
						searchStarted = false;
					}
				}
			}
		}
		block = block.next();
	}
	return emoticons;
}

QMap<int, QString> Emoticons::findImageEmoticons(const QTextDocument *ADocument, int AStartPos, int ALength) const
{
	QMap<int,QString> emoticons;
	QTextBlock block = ADocument->findBlock(AStartPos);
	int stopPos = ALength < 0 ? ADocument->characterCount() : AStartPos+ALength;
	while (block.isValid() && block.position()<stopPos)
	{
		for (QTextBlock::iterator it = block.begin(); !it.atEnd() && it.fragment().position()<stopPos; it++)
		{
			if (it.fragment().charFormat().isImageFormat())
			{
				QString key = FKeyByUrl.value(it.fragment().charFormat().toImageFormat().name());
				if (!key.isEmpty() && it.fragment().length()==1)
					emoticons.insert(it.fragment().position(),key);
			}
		}
		block = block.next();
	}
	return emoticons;
}

void Emoticons::createIconsetUrls()
{
	FUrlByKey.clear();
	FKeyByUrl.clear();
	clearTreeItem(&FRootTreeItem);
	foreach(QString substorage, Options::node(OPV_MESSAGES_EMOTICONS).value().toStringList())
	{
		IconStorage *storage = FStorages.value(substorage);
		if (storage)
		{
			QHash<QString, QString> fileFirstKey;
			foreach(QString key, storage->fileFirstKeys())
				fileFirstKey.insert(storage->fileFullName(key), key);

			foreach(QString key, storage->fileKeys())
			{
				if (!FUrlByKey.contains(key))
				{
					QString file = storage->fileFullName(key);
					QUrl url = QUrl::fromLocalFile(file);
					FUrlByKey.insert(key,url);
					FKeyByUrl.insert(url.toString(),fileFirstKey.value(file));
					createTreeItem(key,url);
				}
			}
		}
	}
}

void Emoticons::createTreeItem(const QString &AKey, const QUrl &AUrl)
{
	EmoticonTreeItem *item = &FRootTreeItem;
	for (int i=0; i<AKey.size(); i++)
	{
		QChar itemChar = AKey.at(i);
		if (!item->childs.contains(itemChar))
		{
			EmoticonTreeItem *childItem = new EmoticonTreeItem;
			item->childs.insert(itemChar,childItem);
			item = childItem;
		}
		else
		{
			item = item->childs.value(itemChar);
		}
	}
	item->url = AUrl;
}

void Emoticons::clearTreeItem(EmoticonTreeItem *AItem) const
{
	foreach(QChar itemChar, AItem->childs.keys())
	{
		EmoticonTreeItem *childItem = AItem->childs.take(itemChar);
		clearTreeItem(childItem);
		delete childItem;
	}
}

bool Emoticons::isWordBoundary(const QString &AText) const
{
	return !AText.isEmpty() ? AText.at(0).isSpace() : true;
}

void Emoticons::replaceTextToImage(QTextDocument *ADocument, int AStartPos, int ALength) const
{
	QMap<int,QString> emoticons = findTextEmoticons(ADocument,AStartPos,ALength);
	if (!emoticons.isEmpty())
	{
		int posOffset = 0;
		QTextCursor cursor(ADocument);
		cursor.beginEditBlock();
		for (QMap<int,QString>::const_iterator it=emoticons.constBegin(); it!=emoticons.constEnd(); it++)
		{
			QUrl url = FUrlByKey.value(it.value());
			if (!url.isEmpty())
			{
				cursor.setPosition(it.key()-posOffset);
				cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor,it->length());
				if (!ADocument->resource(QTextDocument::ImageResource,url).isValid())
					cursor.insertImage(QImage(url.toLocalFile()),url.toString());
				else
					cursor.insertImage(url.toString());
				posOffset += it->length()-1;
			}
		}
		cursor.endEditBlock();
	}
}

void Emoticons::replaceImageToText(QTextDocument *ADocument, int AStartPos, int ALength) const
{
	QMap<int,QString> emoticons = findImageEmoticons(ADocument,AStartPos,ALength);
	if (!emoticons.isEmpty())
	{
		int posOffset = 0;
		QTextCursor cursor(ADocument);
		cursor.beginEditBlock();
		for (QMap<int,QString>::const_iterator it=emoticons.constBegin(); it!=emoticons.constEnd(); it++)
		{
			cursor.setPosition(it.key()+posOffset);
			cursor.deleteChar();
			posOffset--;

			if (cursor.movePosition(QTextCursor::PreviousCharacter,QTextCursor::KeepAnchor,1))
			{
				bool space = !isWordBoundary(cursor.selectedText());
				cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::MoveAnchor,1);
				if (space)
				{
					posOffset++;
					cursor.insertText(" ");
				}
			}

			cursor.insertText(it.value());
			posOffset += it->length();

			if (cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor,1))
			{
				bool space = !isWordBoundary(cursor.selectedText());
				cursor.movePosition(QTextCursor::PreviousCharacter,QTextCursor::MoveAnchor,1);
				if (space)
				{
					posOffset++;
					cursor.insertText(" ");
				}
			}

		}
		cursor.endEditBlock();
	}
}

SelectIconMenu *Emoticons::createSelectIconMenu(const QString &ASubStorage, QWidget *AParent)
{
	SelectIconMenu *menu = new SelectIconMenu(ASubStorage, AParent);
	connect(menu->instance(),SIGNAL(iconSelected(const QString &, const QString &)), SLOT(onIconSelected(const QString &, const QString &)));
	connect(menu->instance(),SIGNAL(destroyed(QObject *)),SLOT(onSelectIconMenuDestroyed(QObject *)));
	return menu;
}

void Emoticons::insertSelectIconMenu(const QString &ASubStorage)
{
	foreach(EmoticonsContainer *container, FContainers)
	{
		SelectIconMenu *menu = createSelectIconMenu(ASubStorage,container);
		FContainerByMenu.insert(menu,container);
		container->insertMenu(menu);
	}
}

void Emoticons::removeSelectIconMenu(const QString &ASubStorage)
{
	QMap<SelectIconMenu *,EmoticonsContainer *>::iterator it = FContainerByMenu.begin();
	while (it != FContainerByMenu.end())
	{
		SelectIconMenu *menu = it.key();
		if (menu->iconset() == ASubStorage)
		{
			it.value()->removeMenu(menu);
			it = FContainerByMenu.erase(it);
			delete menu;
		}
		else
			it++;
	}
}

#ifdef Q_WS_MAC
bool Emoticons::eventFilter(QObject * obj, QEvent * evt)
{
	if (QTextEdit * te = qobject_cast<QTextEdit*>(obj))
	{
		switch (evt->type())
		{
		case QEvent::FocusIn:
			{
				currentEditWidget = qobject_cast<IEditWidget*>(te->parentWidget());
				emoticonsMenu->setEnabled(true);
				break;
			}
		case QEvent::FocusOut:
			{
				currentEditWidget = NULL;
				emoticonsMenu->setEnabled(false);
				break;
			}
		default:
			break;
		}
	}
	return QObject::eventFilter(obj, evt);
}

void Emoticons::onEmoticonAction()
{
	Action * action = qobject_cast<Action*>(sender());
	if (action)
	{
		QString substorage = action->data(Action::DR_UserDefined + 3).toString();
		QString key = action->data(Action::DR_UserDefined + 4).toString();
		onIconSelected(substorage, key);
	}
}

#endif

void Emoticons::onEditWidgetCreated(IEditWidget *AEditWidget)
{
	EmoticonsContainer *container = new EmoticonsContainer(AEditWidget);
	container->setObjectName("emoticonsContainer");
	FContainers.append(container);

	foreach(QString substorage, activeIconsets())
	{
		SelectIconMenu *menu = createSelectIconMenu(substorage,container);
		container->insertMenu(menu);
		FContainerByMenu.insert(menu,container);
	}

	QHBoxLayout *hlayout = qobject_cast<QHBoxLayout*>(AEditWidget->textEdit()->layout());
	if (hlayout)
	{
		QVBoxLayout *vlayout = NULL;
		for (int i = 0; i < hlayout->count(); i++)
		{
			if ((vlayout = qobject_cast<QVBoxLayout*>(hlayout->itemAt(i)->layout())))
				vlayout->insertWidget(0, container,0,Qt::AlignTop);
		}
	}

#ifdef Q_WS_MAC
	AEditWidget->textEdit()->installEventFilter(this);
#endif

	connect(AEditWidget->textEdit()->document(),SIGNAL(contentsChange(int,int,int)),SLOT(onEditWidgetContentsChanged(int,int,int)));
	connect(container,SIGNAL(destroyed(QObject *)),SLOT(onEmoticonsContainerDestroyed(QObject *)));
}

void Emoticons::onEditWidgetContentsChanged(int APosition, int ARemoved, int AAdded)
{
	Q_UNUSED(ARemoved);
	if (AAdded>0)
	{
		QTextDocument *doc = qobject_cast<QTextDocument *>(sender());
		QList<QUrl> urlList = FUrlByKey.values();
		QTextBlock block = doc->findBlock(APosition);
		while (block.isValid() && block.position()<=APosition+AAdded)
		{
			for (QTextBlock::iterator it = block.begin(); !it.atEnd(); it++)
			{
				QTextFragment fragment = it.fragment();
				if (fragment.charFormat().isImageFormat())
				{
					QUrl url = fragment.charFormat().toImageFormat().name();
					if (doc->resource(QTextDocument::ImageResource,url).isNull())
					{
						if (urlList.contains(url))
						{
							doc->addResource(QTextDocument::ImageResource,url,QImage(url.toLocalFile()));
							doc->markContentsDirty(fragment.position(),fragment.length());
						}
					}
				}
			}
			block = block.next();
		}
	}
}

void Emoticons::onEmoticonsContainerDestroyed(QObject *AObject)
{
	QList<EmoticonsContainer *>::iterator it = FContainers.begin();
	while (it != FContainers.end())
	{
		if (qobject_cast<QObject *>(*it) == AObject)
			it = FContainers.erase(it);
		else
			it++;
	}
}

void Emoticons::onSelectIconMenuDestroyed(QObject *AObject)
{
	foreach(SelectIconMenu *menu, FContainerByMenu.keys())
		if (qobject_cast<QObject *>(menu) == AObject)
			FContainerByMenu.remove(menu);
}

void Emoticons::onIconSelected(const QString &ASubStorage, const QString &AIconKey)
{
	Q_UNUSED(ASubStorage);
	SelectIconMenu *menu = qobject_cast<SelectIconMenu *>(sender());
	IEditWidget *widget = NULL;
	if (FContainerByMenu.contains(menu))
	{
		widget = FContainerByMenu.value(menu)->editWidget();
	}
#ifdef Q_WS_MAC
	else
		widget = currentEditWidget;
#endif
	if (widget)
	{
		QTextEdit *editor = widget->textEdit();
		editor->textCursor().beginEditBlock();
		editor->textCursor().insertText(AIconKey);
		editor->textCursor().insertText(" ");
		editor->textCursor().endEditBlock();
		editor->setFocus();
	}
}

void Emoticons::onOptionsOpened()
{
	onOptionsChanged(Options::node(OPV_MESSAGES_EMOTICONS));
	onOptionsChanged(Options::node(OPV_MESSAGES_EMOTICONS_ENABLED));
}

void Emoticons::onOptionsChanged(const OptionsNode &ANode)
{
	if (ANode.path() == OPV_MESSAGES_EMOTICONS_ENABLED)
	{
		if (FMessageProcessor)
		{
			if (ANode.value().toBool())
				FMessageProcessor->insertMessageWriter(MWO_EMOTICONS,this);
			else
				FMessageProcessor->removeMessageWriter(MWO_EMOTICONS,this);
		}
	}
	else if (ANode.path() == OPV_MESSAGES_EMOTICONS)
	{
		QList<QString> oldStorages = FStorages.keys();
		QList<QString> availStorages = IconStorage::availSubStorages(RSR_STORAGE_EMOTICONS);

		foreach(QString substorage, Options::node(OPV_MESSAGES_EMOTICONS).value().toStringList())
		{
			if (availStorages.contains(substorage))
			{
				if (!FStorages.contains(substorage))
				{
					FStorages.insert(substorage, new IconStorage(RSR_STORAGE_EMOTICONS,substorage,this));
					insertSelectIconMenu(substorage);
				}
				oldStorages.removeAll(substorage);
			}
		}

		foreach (QString substorage, oldStorages)
		{
			removeSelectIconMenu(substorage);
			delete FStorages.take(substorage);
		}

#ifdef Q_WS_MAC
		emoticonsMenu->clear();
		foreach(Action * a, emoticonsActions)
			a->deleteLater();

		emoticonsActions.clear();

		QList<Action*> tmp;
		foreach(IconStorage * storage, FStorages.values())
		{
			foreach (QString key, storage->fileFirstKeys())
			{
				Action * action = new Action;
				action->setText(storage->fileOption(key, "comment"));
				action->setIcon(storage->getIcon(key));
				action->setData(Action::DR_UserDefined + 3, storage->storage());
				action->setData(Action::DR_UserDefined + 4, key);
				connect(action, SIGNAL(triggered()), SLOT(onEmoticonAction()));
				tmp << action;
			}
		}

		emoticonsActions = reversed<Action*>(tmp);

		emoticonsMenu->addActions(emoticonsActions, -1);
#endif

		createIconsetUrls();
	}
}

Q_EXPORT_PLUGIN2(plg_emoticons, Emoticons)
