#include "Dialog.hh"
#include "EditVisitor.hh"
#include "PrintResult.hh"
#include "ManageDistributionsDialog.hh"
#include "ManageEventsDialog.hh"

MainWindow::MainWindow() : editor(nullptr), fileManager(nullptr)
{
	statusBar();
	createActions();
	createMenus();
	createToolBar();

	setWindowTitle("FTEdit");
	setWindowIcon(QIcon(":icons/ftedit.ico"));
	QRect r = QGuiApplication::primaryScreen()->geometry();
	setMinimumSize(MIN(r.width(), RES_MIN_X), MIN(r.height(), RES_MIN_Y));
	resize(r.width() / 1.5, r.height() / 1.5);

	auto centralwidget = new QWidget(this);
	setCentralWidget(centralwidget);
	centralwidget->setStyleSheet("selection-background-color: #4684e3;");
	auto gridLayout = new QGridLayout(centralwidget);
	gridLayout->setMargin(0);
	hSplitter = new QSplitter(centralwidget);
	hSplitter->setOrientation(Qt::Horizontal);
	auto horizontalLayout = new QWidget(hSplitter);
	auto explorerLayout = new QHBoxLayout(horizontalLayout);
	explorerLayout->setContentsMargins(0, 0, 0, 0);
	explorer = new QTreeWidget(horizontalLayout);
	explorer->headerItem()->setText(0, "Project Explorer");
	explorer->setContextMenuPolicy(Qt::CustomContextMenu);
	explorerLayout->addWidget(explorer);

	trees = new QTreeWidgetItem(explorer);
	trees->setText(0, "Fault tree");
	trees->setExpanded(true);
	explorer->addTopLevelItem(trees);
	results = new QTreeWidgetItem(explorer);
	results->setText(0, "Result");
	explorer->addTopLevelItem(results);

	hSplitter->addWidget(horizontalLayout);
	vSplitter = new QSplitter(hSplitter);
	vSplitter->setOrientation(Qt::Vertical);
	auto verticalLayout = new QWidget(vSplitter);
	auto viewLayout = new QVBoxLayout(verticalLayout);
	view = new GraphicsView(verticalLayout);
	viewLayout->setContentsMargins(0, 0, 0, 0);
	scene = new QGraphicsScene(this);
	view->setScene(scene);

	viewLayout->addWidget(view);

	vSplitter->addWidget(verticalLayout);
	auto verticalLayout2 = new QWidget(vSplitter);
	auto groupBoxLayout = new QVBoxLayout(verticalLayout2);
	groupBoxLayout->setContentsMargins(0, 0, 0, 0);
	auto groupBox = new QGroupBox(verticalLayout2);
	groupBox->setTitle("Error list");
	groupBoxLayout->addWidget(groupBox);
	auto gridLayout2 = new QGridLayout(groupBox);
	gridLayout2->setContentsMargins(2, 2, 2, 2);
	errorList = new ListWidget(groupBox);
	errorList->setStyleSheet("selection-background-color: #de0b0b;");
	errorList->setSelectionMode(QAbstractItemView::SingleSelection);
	gridLayout2->addWidget(errorList, 0, 0, 1, 1);

	vSplitter->addWidget(verticalLayout2);
	hSplitter->addWidget(vSplitter);
	gridLayout->addWidget(hSplitter, 0, 0, 1, 1);
	explorer->resize(0, explorer->height());
	toggleExplorer();
	errorList->resize(errorList->width(), 1);
	toggleErrorList();
	fileManager = new FileManagerSystem;
	this->newFile();

	connect(scene, SIGNAL(selectionChanged()), this, SLOT(changeItem()));
	connect(explorer, SIGNAL(itemClicked(QTreeWidgetItem *, int)),
	this, SLOT(explorerItemClicked(QTreeWidgetItem *, int)));
	connect(explorer, SIGNAL(customContextMenuRequested(const QPoint &)),
	this, SLOT(explorerShowContextMenu(const QPoint &)));
}

void MainWindow::newFile()
{
	if (!maybeSave())
		return ;
	scene->clear();
	reset();
	editor = new Editor();
	editor->detach(); // Creates empty tree
	setEnabledButton();
	QTreeWidgetItem *tree1 = new QTreeWidgetItem(trees);
	tree1->setText(0, " * " + editor->getSelection()->getProperties().getName());
	trees->addChild(tree1);
	curTreeRow = 0;
}

void MainWindow::open()
{
	bool oldModified = modified;
	if (!maybeSave())
		return ;
	QString path = QFileDialog::getOpenFileName(this, "Open project", QDir::homePath(),
	"Open-PSA project (*.opsa);;All Files (*)");
	if (path.isEmpty())
		return ;
	auto newEditor = fileManager->load(path);
	if (newEditor)
	{
		delete editor;
		editor = newEditor;
		editor->setAutoRefresh(true);
		editor->refresh();
		return ;
	}
	QMessageBox msg(this);
	msg.setIcon(QMessageBox::Critical);
	msg.setWindowTitle("Error");
	msg.setText(fileManager->getErrorMessage());
	msg.exec();
	modified = oldModified;
}

void MainWindow::save()
{
	if (fileManager->getPath().isEmpty())
	{
		saveAs();
		return ;
	}
	fileManager->save(editor);
	if (fileManager->getErrorMessage().isEmpty())
	{
		modified = false; // si pas d'erreurs
		return ;
	}
	QMessageBox msg(this);
	msg.setIcon(QMessageBox::Critical);
	msg.setWindowTitle("Error");
	msg.setText(fileManager->getErrorMessage());
}

void MainWindow::saveAs()
{
	QString path;
	while (1)
	{
		path = QFileDialog::getSaveFileName(this, "Save project as", QDir::homePath(), "Open-PSA project (*.opsa)");
		if (path.isEmpty()) return ;
		if (!path.endsWith(".opsa"))
			path += ".opsa";
		fileManager->saveAs(path, editor);
		if (fileManager->getErrorMessage().isEmpty())
		{
			modified = false; // si pas d'erreurs
			break ;
		}
		QMessageBox msg(this);
		msg.setIcon(QMessageBox::Critical);
		msg.setStandardButtons(QMessageBox::Retry | QMessageBox::Cancel);
		msg.setWindowTitle("Error");
		msg.setText(fileManager->getErrorMessage());
		int ret = msg.exec();
		if (ret == QMessageBox::Cancel)
		{
			fileManager->setPath("");
			break ;
		}
	}
}

void MainWindow::closeEvent(QCloseEvent* e)
{
	if (!maybeSave() || modified)
	{
		e->ignore();
		return ;
	}
	QMainWindow::closeEvent(e);
}

void MainWindow::cut()
{
	Node *parent = curItem->node()->getParent();
	editor->cut(curItem->node());
	updateScene(parent);
	modified = true;
}

void MainWindow::copy()
{
	editor->copy(curItem->node());
	updateScene(curItem->node());
}

void MainWindow::paste()
{
	editor->paste(curItem ? (Gate*)curItem->node() : nullptr);
	updateScene(curItem ? curItem->node() : editor->getSelection()->getTop());
	modified = true;
}

void MainWindow::addAnd()
{
	addGate(new And(editor->generateName(PREFIX_GATE)));
}

void MainWindow::addInhibit()
{
	addGate(new Inhibit(editor->generateName(PREFIX_GATE)));
}

void MainWindow::addOr()
{
	addGate(new Or(editor->generateName(PREFIX_GATE)));
}

void MainWindow::addKN()
{
	addGate(new VotingOR(editor->generateName(PREFIX_GATE)));
}

void MainWindow::addXor()
{
	addGate(new Xor(editor->generateName(PREFIX_GATE)));
}

void MainWindow::addEvent()
{
	modified = true;
	QList<Event> &events = editor->getEvents();
	events << Event(editor->generateName(PREFIX_EVENT));
	auto cont = new Container(&events.last());
	cont->attach((Gate*)curItem->node());
	updateScene(curItem->node());
}

void MainWindow::addTransfert()
{
	modified = true;
	auto t = new Transfert();
	t->attach((Gate*)curItem->node());
	updateScene(curItem->node());
}

void MainWindow::zoomIn()
{
	qreal f = 1 + ZOOM_STEP;
	f * view->zoom() > ZOOM_MAX ? view->setZoom(ZOOM_MAX) : view->scale(f, f);
}

void MainWindow::zoomOut()
{
	qreal f = 1 - ZOOM_STEP;
	f * view->zoom() < ZOOM_MIN ? view->setZoom(ZOOM_MIN) : view->scale(f, f);
}

void MainWindow::zoomReset()
{
	view->setZoom(1);
}

void MainWindow::showToolBar()
{
	if (showToolBarAct->isChecked())
		toolBar->show();
	else
		toolBar->hide();
}

void MainWindow::toggleExplorer()
{
	if (explorer->width())
	{
		resizeSplitter(hSplitter, 0, errorList->width());
		return ;
	}
	QSize size = this->size();
	int width = size.width() / 6;
	resizeSplitter(hSplitter, width, 4 * width);
}

void MainWindow::toggleErrorList()
{
	if (errorList->height())
	{
		resizeSplitter(vSplitter, view->height(), 0);
		return ;
	}
	QSize size = this->size();
	int height = size.height() / 3;
	resizeSplitter(vSplitter, 2 * height, height);
}

void MainWindow::showDistributions()
{
	modified = true;
	ManageDistributionsDialog(this, editor->getDistributions()).exec();
	editor->refresh(); // Remove unused Distributions
}

void MainWindow::showEvents()
{
	modified = true;
	ManageEventsDialog(this, editor->getEvents()).exec();
	editor->refresh(); // Remove unused Events
}

void MainWindow::evaluate()
{
	if (ChooseResultDialog(this, (Gate*)curItem->node(), resultsHistory).exec() == QDialog::Rejected)
		return ; // cancel
	Result *result = resultsHistory.last();
	if (result->getErrors().size()) // invalid tree
	{
		errorList->clear();
		errorList->addItems(QStringList(result->getErrors()));
		if (!errorList->height())
			toggleErrorList(); // show list
		resultsHistory.removeLast(); // Discard result
		QMessageBox msg(this);
		msg.setIcon(QMessageBox::Warning);
		msg.setWindowTitle("Error");
		msg.setText("The analysis could not be performed due to errors detected in the fault tree."
		"\n\nPlease correct these errors before starting the analysis.");
		msg.exec();
		return ;
	}
	if (!result->getResultBoolean() && !result->getResultMCS())
	{
		resultsHistory.removeLast(); // Discard result
		QMessageBox msg(this);
		msg.setIcon(QMessageBox::Information);
		msg.setWindowTitle("Information");
		msg.setText("Fault tree check completed successfully.");
		msg.exec();
		return ;
	}
	auto *resultItem = new QTreeWidgetItem(results);
	results->addChild(resultItem);
	resultItem->setText(0, QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
}

void MainWindow::about()
{
	QMessageBox about(this);
	about.setWindowIcon(QIcon(":icons/about.png"));
	about.setWindowTitle("About FTEdit");
	about.setText("FTEdit is an open source editor fault tree analysis tool.<br><br>"
	"License: GPLv3<br>Source code is available on "
	"<a href='https://github.com/ChuOkupai/FTEdit'>GitHub</a><br>");
	about.exec();
}

void MainWindow::moveItemFirst()
{
	QList<Node*> &l = curItem->node()->getParent()->getChildren();
	l.move(l.indexOf(curItem->node()), 0);
	updateScene(curItem->node());
}

void MainWindow::moveItemLeft()
{
	QList<Node*> &l = curItem->node()->getParent()->getChildren();
	int pos = l.indexOf(curItem->node());
	l.move(pos, pos - 1);
	updateScene(curItem->node());
}

void MainWindow::moveItemRight()
{
	QList<Node*> &l = curItem->node()->getParent()->getChildren();
	int pos = l.indexOf(curItem->node());
	l.move(pos, pos + 1);
	updateScene(curItem->node());
}

void MainWindow::moveItemLast()
{
	QList<Node*> &l = curItem->node()->getParent()->getChildren();
	l.move(l.indexOf(curItem->node()), l.size() - 1);
	updateScene(curItem->node());
}

void MainWindow::editItem()
{
	EditVisitor visitor(this, *editor, curItem);
	curItem->node()->accept(visitor);
	scene->update();
}

void MainWindow::removeItem()
{
	Node *parent = curItem->node()->getParent();
	editor->remove(curItem->node());
	editor->refresh();
	updateScene(parent);
}

void MainWindow::detach()
{
	editor->detach((Gate*)curItem->node());
	auto t = new QTreeWidgetItem(trees);
	t->setText(0, editor->getSelection()->getProperties().getName());
	trees->addChild(t);
	explorerItemClicked(t, 0);
}

void MainWindow::join()
{
	int index;
	if (ChooseTreeDialog(this, *editor, index).exec() == QDialog::Rejected)
		return ; // cancel
	Tree *tree = &editor->getTrees()[index];
	if (tree->getProperties().getRefCount())
	{
		QMessageBox msg(this);
		msg.setIcon(QMessageBox::Critical);
		msg.setWindowTitle("Error");
		msg.setText("Could not merge a fault tree already linked to a transfer in gate."
		"\n\nPlease remove the link in the transfer in gate first before merging the tree");
		msg.exec();
		return ;
	}
	if (index < curTreeRow)
		--curTreeRow;
	editor->join(tree, (Gate*)curItem->node());
	auto child = trees->child(index);
	trees->removeChild(child);
	delete child;
	updateScene(curItem->node());
}

void MainWindow::changeItem()
{
	QList<QGraphicsItem*> list = scene->selectedItems();
	if (list.size() != 1)
	{
		curItem->setSelected(!list.size());
		return ;
	}
	curItem = (NodeItem*)list[0];
	setEnabledButton();
}

void MainWindow::explorerItemClicked(QTreeWidgetItem *item, int column)
{
	(void)column;
	if (item->parent())
	{
		if (explorer->indexOfTopLevelItem(item->parent())) // == 1 (Results)
			PrintResult(this, resultsHistory[results->indexOfChild(item)]).exec();
		else // Fault tree
		{
			auto child = trees->child(curTreeRow);
			if (child == item) return ;
			child->setText(0, child->text(0).mid(3));
			curTreeRow = trees->indexOfChild(item);
			item->setText(0, " * " + item->text(0));
			editor->setSelection(&editor->getTrees()[curTreeRow]);
			updateScene(editor->getSelection()->getTop());
		}
	}
}

void MainWindow::explorerShowContextMenu(const QPoint &pos)
{
	auto item = explorer->itemAt(pos);
	if (!item || !item->parent()) return ;
	QMenu menu;
	if (explorer->topLevelItem(0) == item->parent())
	{
		menu.addAction(editTreePropertiesAct);
		menu.addAction(removeTreeAct);
		selectedRow = trees->indexOfChild(item);
		removeTreeAct->setDisabled(selectedRow == curTreeRow ||
		editor->getTrees()[selectedRow].getProperties().getRefCount());
	}
	else
	{
		selectedRow = results->indexOfChild(item);
		menu.addAction(removeResultAct);
	}
	menu.exec(QCursor::pos());
}

void MainWindow::editTreeProperties()
{
	auto tree = &editor->getTrees()[selectedRow];
	PropertiesDialog(this, *editor, &tree->getProperties()).exec();
	auto treeItem = trees->child(selectedRow);
	if (curTreeRow == selectedRow)
		treeItem->setText(0, " * " + tree->getProperties().getName());
	else
		treeItem->setText(0, tree->getProperties().getName());
}

void MainWindow::removeTree()
{
	auto tree = &editor->getTrees()[selectedRow];
	if (tree->getTop())
	{
		tree->getTop()->remove();
		tree->setTop(nullptr);
	}
	editor->getTrees().removeAt(selectedRow);
	editor->refresh();
	if (selectedRow < curTreeRow)
		--curTreeRow;
	auto child = trees->child(selectedRow);
	trees->removeChild(child);
	delete child;
}

void MainWindow::removeResult()
{
	delete resultsHistory[selectedRow];
	resultsHistory.removeAt(selectedRow);
	auto child = results->child(selectedRow);
	results->removeChild(child);
	delete child;
}

void MainWindow::createActions()
{
	newAct = new QAction("&New project", this);
	newAct->setShortcuts(QKeySequence::New);
	newAct->setStatusTip("Create a new project");
	newAct->setToolTip(newAct->statusTip());
	newAct->setIcon(QIcon(":icons/new.png"));
	connect(newAct, &QAction::triggered, this, &MainWindow::newFile);

	openAct = new QAction("&Open...", this);
	openAct->setShortcuts(QKeySequence::Open);
	openAct->setStatusTip("Open an existing project");
	openAct->setToolTip(openAct->statusTip());
	openAct->setIcon(QIcon(":icons/open.png"));
	connect(openAct, &QAction::triggered, this, &MainWindow::open);

	saveAct = new QAction("&Save", this);
	saveAct->setShortcuts(QKeySequence::Save);
	saveAct->setStatusTip("Save the project to disk");
	saveAct->setToolTip(saveAct->statusTip());
	saveAct->setIcon(QIcon(":icons/save.png"));
	connect(saveAct, &QAction::triggered, this, &MainWindow::save);

	saveAsAct = new QAction("Save As...", this);
	saveAsAct->setShortcuts(QKeySequence::SaveAs);
	saveAsAct->setStatusTip("Save the project to disk with a different name");
	saveAsAct->setToolTip(saveAsAct->statusTip());
	saveAsAct->setIcon(QIcon(":icons/saveAs.png"));
	connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveAs);

	exitAct = new QAction("E&xit", this);
	exitAct->setShortcuts(QKeySequence::Quit);
	exitAct->setStatusTip("Exit the application");
	exitAct->setToolTip(exitAct->statusTip());
	exitAct->setIcon(QIcon(":icons/exit.png"));
	connect(exitAct, &QAction::triggered, this, &QWidget::close);

	cutAct = new QAction("Cu&t", this);
	cutAct->setShortcuts(QKeySequence::Cut);
	cutAct->setStatusTip("Cut the current selected node to the clipboard");
	cutAct->setToolTip(cutAct->statusTip());
	cutAct->setIcon(QIcon(":icons/cut.png"));
	connect(cutAct, &QAction::triggered, this, &MainWindow::cut);

	copyAct = new QAction("&Copy", this);
	copyAct->setShortcuts(QKeySequence::Copy);
	copyAct->setStatusTip("Copy the current selected node to the clipboard");
	copyAct->setToolTip(copyAct->statusTip());
	copyAct->setIcon(QIcon(":icons/copy.png"));
	connect(copyAct, &QAction::triggered, this, &MainWindow::copy);

	pasteAct = new QAction("&Paste", this);
	pasteAct->setShortcuts(QKeySequence::Paste);
	pasteAct->setStatusTip("Paste the clipboard's contents into the current node");
	pasteAct->setToolTip(pasteAct->statusTip());
	pasteAct->setIcon(QIcon(":icons/paste.png"));
	connect(pasteAct, &QAction::triggered, this, &MainWindow::paste);

	addAndAct = new QAction("And", this);
	addAndAct->setStatusTip("Add a new and gate into the current tree");
	addAndAct->setToolTip(addAndAct->statusTip());
	addAndAct->setIcon(QIcon(":objects/and.png"));
	connect(addAndAct, &QAction::triggered, this, &MainWindow::addAnd);

	addInhibitAct = new QAction("Inhibit", this);
	addInhibitAct->setStatusTip("Add a new inhibit gate into the current tree");
	addInhibitAct->setToolTip(addInhibitAct->statusTip());
	addInhibitAct->setIcon(QIcon(":objects/inhibit.png"));
	connect(addInhibitAct, &QAction::triggered, this, &MainWindow::addInhibit);

	addOrAct = new QAction("Or", this);
	addOrAct->setStatusTip("Add a new or gate into the current tree");
	addOrAct->setToolTip(addOrAct->statusTip());
	addOrAct->setIcon(QIcon(":objects/or.png"));
	connect(addOrAct, &QAction::triggered, this, &MainWindow::addOr);

	addKNAct = new QAction("Voting or", this);
	addKNAct->setStatusTip("Add a new voting or gate into the current tree");
	addKNAct->setToolTip(addKNAct->statusTip());
	addKNAct->setIcon(QIcon(":objects/kn.png"));
	connect(addKNAct, &QAction::triggered, this, &MainWindow::addKN);

	addXorAct = new QAction("Xor", this);
	addXorAct->setStatusTip("Add a new xor gate into the current tree");
	addXorAct->setToolTip(addXorAct->statusTip());
	addXorAct->setIcon(QIcon(":objects/xor.png"));
	connect(addXorAct, &QAction::triggered, this, &MainWindow::addXor);

	addTransfertAct = new QAction("Add transfert in", this);
	addTransfertAct->setStatusTip("Add a new transfert in into the current tree");
	addTransfertAct->setToolTip(addTransfertAct->statusTip());
	addTransfertAct->setIcon(QIcon(":objects/transfert.png"));
	connect(addTransfertAct, &QAction::triggered, this, &MainWindow::addTransfert);

	addEventAct = new QAction("Add basic event", this);
	addEventAct->setStatusTip("Add a new basic event into the current tree");
	addEventAct->setToolTip(addEventAct->statusTip());
	addEventAct->setIcon(QIcon(":objects/basicEvent.png"));
	connect(addEventAct, &QAction::triggered, this, &MainWindow::addEvent);

	zoomInAct = new QAction("Zoom In", this); zoomInAct->setShortcuts(QKeySequence::ZoomIn);
	zoomInAct->setStatusTip("Zoom In");
	zoomInAct->setIcon(QIcon(":icons/zoomIn.png"));
	connect(zoomInAct, &QAction::triggered, this, &MainWindow::zoomIn);

	zoomOutAct = new QAction("Zoom Out", this); zoomOutAct->setShortcuts(QKeySequence::ZoomOut);
	zoomOutAct->setStatusTip("Zoom Out");
	zoomOutAct->setIcon(QIcon(":icons/zoomOut.png"));
	connect(zoomOutAct, &QAction::triggered, this, &MainWindow::zoomOut);

	zoomResetAct = new QAction("Reset Zoom", this);
	zoomResetAct->setStatusTip("Reset Zoom");
	zoomResetAct->setIcon(QIcon(":icons/zoomReset.png"));
	connect(zoomResetAct, &QAction::triggered, this, &MainWindow::zoomReset);

	showToolBarAct = new QAction("Show Toolbar", this);
	showToolBarAct->setStatusTip("Enable or disable Toolbar");
	showToolBarAct->setCheckable(true);
	showToolBarAct->setChecked(true);
	connect(showToolBarAct, &QAction::triggered, this, &MainWindow::showToolBar);

	toggleExplorerAct = new QAction("Toggle Project Explorer", this);
	toggleExplorerAct->setStatusTip("Display or hide Project Explorer section");
	toggleExplorerAct->setIcon(QIcon(":icons/edit.png"));
	connect(toggleExplorerAct, &QAction::triggered, this, &MainWindow::toggleExplorer);

	toggleErrorListAct = new QAction("Toggle Error List", this);
	toggleErrorListAct->setStatusTip("Display or hide Error List section");
	toggleErrorListAct->setIcon(QIcon(":icons/edit.png"));
	connect(toggleErrorListAct, &QAction::triggered, this, &MainWindow::toggleErrorList);

	distributionsAct = new QAction("Manage distributions...", this);
	distributionsAct->setStatusTip("Show the list of all distributions");
	distributionsAct->setIcon(QIcon(":icons/manage.png"));
	connect(distributionsAct, &QAction::triggered, this, &MainWindow::showDistributions);

	eventsAct = new QAction("Manage events...", this);
	eventsAct->setStatusTip("Show the list of all events");
	eventsAct->setIcon(QIcon(":icons/manage.png"));
	connect(eventsAct, &QAction::triggered, this, &MainWindow::showEvents);

	evaluateAct = new QAction("Evaluate...", this);
	evaluateAct->setStatusTip("Perform a fault tree analysis");
	evaluateAct->setToolTip(evaluateAct->statusTip());
	evaluateAct->setIcon(QIcon(":icons/evaluate.png"));
	connect(evaluateAct, &QAction::triggered, this, &MainWindow::evaluate);

	aboutAct = new QAction("About", this);
	aboutAct->setStatusTip("About FTEdit");
	aboutAct->setIcon(QIcon(":icons/about.png"));
	connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

	aboutQtAct = new QAction("About Qt", this);
	aboutQtAct->setStatusTip("About Qt");
	aboutQtAct->setIcon(QIcon(":icons/about.png"));
	connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);

	moveItemFirstAct = new QAction("Move First", this);
	moveItemFirstAct->setStatusTip("Move the node to the leftmost position");
	moveItemFirstAct->setIcon(QIcon(":icons/moveFirst.png"));
	connect(moveItemFirstAct, &QAction::triggered, this, &MainWindow::moveItemFirst);

	moveItemLeftAct = new QAction("Move Left", this);
	moveItemLeftAct->setStatusTip("Move the node to the left");
	moveItemLeftAct->setIcon(QIcon(":icons/moveLeft.png"));
	connect(moveItemLeftAct, &QAction::triggered, this, &MainWindow::moveItemLeft);

	moveItemRightAct = new QAction("Move Right", this);
	moveItemRightAct->setStatusTip("Move the node to the right");
	moveItemRightAct->setIcon(QIcon(":icons/moveRight.png"));
	connect(moveItemRightAct, &QAction::triggered, this, &MainWindow::moveItemRight);

	moveItemLastAct = new QAction("Move Last", this);
	moveItemLastAct->setStatusTip("Move the node to the rightmost position");
	moveItemLastAct->setIcon(QIcon(":icons/moveLast.png"));
	connect(moveItemLastAct, &QAction::triggered, this, &MainWindow::moveItemLast);

	editItemAct = new QAction("Properties", this);
	editItemAct->setStatusTip("Edit the node properties");
	editItemAct->setIcon(QIcon(":icons/edit.png"));
	connect(editItemAct, &QAction::triggered, this, &MainWindow::editItem);

	removeItemAct = new QAction("Remove", this);
	removeItemAct->setStatusTip("Remove all nodes starting from here");
	removeItemAct->setIcon(QIcon(":icons/remove.png"));
	connect(removeItemAct, &QAction::triggered, this, &MainWindow::removeItem);

	detachItemAct = new QAction("Detach", this);
	detachItemAct->setStatusTip("Move all nodes starting from here in a new fault tree");
	detachItemAct->setIcon(QIcon(":icons/detach.png"));
	connect(detachItemAct, &QAction::triggered, this, &MainWindow::detach);

	joinItemAct = new QAction("Join", this);
	joinItemAct->setStatusTip("Merge the content of a fault tree as a child of this node");
	joinItemAct->setIcon(QIcon(":icons/add.png"));
	connect(joinItemAct, &QAction::triggered, this, &MainWindow::join);

	editTreePropertiesAct = new QAction("Properties", this);
	editTreePropertiesAct->setStatusTip("Edit the properties of the fault tree");
	editTreePropertiesAct->setIcon(QIcon(":icons/edit.png"));
	connect(editTreePropertiesAct, &QAction::triggered, this, &MainWindow::editTreeProperties);

	removeTreeAct = new QAction("Delete", this);
	removeTreeAct->setStatusTip("Delete the fault tree and its content");
	removeTreeAct->setIcon(QIcon(":icons/remove.png"));
	connect(removeTreeAct, &QAction::triggered, this, &MainWindow::removeTree);

	removeResultAct = new QAction("Delete", this);
	removeResultAct->setStatusTip("Delete fault tree analysis results");
	removeResultAct->setIcon(QIcon(":icons/remove.png"));
	connect(removeResultAct, &QAction::triggered, this, &MainWindow::removeResult);
}

void MainWindow::createMenus()
{
	QMenu *m;

	menuBar()->setContextMenuPolicy(Qt::PreventContextMenu);

	m = menuBar()->addMenu("&File");
	m->addAction(newAct);
	m->addAction(openAct);
	m->addAction(saveAct);
	m->addAction(saveAsAct);
	m->addSeparator();
	m->addAction(exitAct);

	m = menuBar()->addMenu("&Edit");
	m->addAction(cutAct);
	m->addAction(copyAct);
	m->addAction(pasteAct);
	m->addSeparator();
	gatesMenu = m->addMenu("Add Gate...");
	gatesMenu->setIcon(QIcon(":icons/add.png"));
	gatesMenu->addAction(addAndAct);
	gatesMenu->addAction(addInhibitAct);
	gatesMenu->addAction(addOrAct);
	gatesMenu->addAction(addKNAct);
	gatesMenu->addAction(addXorAct);
	m->addAction(addTransfertAct);
	m->addAction(addEventAct);
	m->addSeparator();
	m->addAction(moveItemFirstAct);
	m->addAction(moveItemLeftAct);
	m->addAction(moveItemRightAct);
	m->addAction(moveItemLastAct);
	m->addSeparator();
	m->addAction(editItemAct);
	m->addAction(removeItemAct);
	m->addAction(detachItemAct);
	m->addAction(joinItemAct);

	m = menuBar()->addMenu("&View");
	m->addAction(zoomInAct);
	m->addAction(zoomOutAct);
	m->addAction(zoomResetAct);
	m->addSeparator();
	m->addAction(showToolBarAct);
	m->addAction(toggleExplorerAct);
	m->addAction(toggleErrorListAct);

	m = menuBar()->addMenu("&Show");
	m->addAction(distributionsAct);
	m->addAction(eventsAct);

	m = menuBar()->addMenu("&Analysis");
	m->addAction(evaluateAct);

	m = menuBar()->addMenu("&Help");
	m->addAction(aboutAct);
	m->addAction(aboutQtAct);

	itemsMenu = new QMenu(this);
	itemsMenu->addAction(moveItemFirstAct);
	itemsMenu->addAction(moveItemLeftAct);
	itemsMenu->addAction(moveItemRightAct);
	itemsMenu->addAction(moveItemLastAct);
	itemsMenu->addSeparator();
	itemsMenu->addAction(editItemAct);
	itemsMenu->addAction(removeItemAct);
	itemsMenu->addAction(detachItemAct);
	itemsMenu->addAction(joinItemAct);
}

void MainWindow::createToolBar()
{
	toolBar = new QToolBar;
	toolBar->setFloatable(false);
	toolBar->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
	toolBar->setContextMenuPolicy(Qt::PreventContextMenu);
	addToolBar(Qt::TopToolBarArea, toolBar);

	toolBar->addAction(newAct);
	toolBar->addAction(openAct);
	toolBar->addAction(saveAct);
	toolBar->addAction(saveAsAct);
	toolBar->addSeparator();
	toolBar->addAction(cutAct);
	toolBar->addAction(copyAct);
	toolBar->addAction(pasteAct);
	toolBar->addSeparator();
	auto button = new GateToolButton(this);
	button->setMenu(gatesMenu);
	button->setDefaultAction(addAndAct);
	toolBar->addWidget(button);
	toolBar->addAction(addTransfertAct);
	toolBar->addAction(addEventAct);
	toolBar->addSeparator();
	toolBar->addAction(moveItemFirstAct);
	toolBar->addAction(moveItemLeftAct);
	toolBar->addAction(moveItemRightAct);
	toolBar->addAction(moveItemLastAct);
	toolBar->addSeparator();
	toolBar->addAction(zoomInAct);
	toolBar->addAction(zoomOutAct);
	toolBar->addAction(zoomResetAct);
	toolBar->addSeparator();
	toolBar->addAction(evaluateAct);
}

bool MainWindow::maybeSave()
{
	if (!modified)
		return (true);
	const QMessageBox::StandardButton ret = QMessageBox::warning(this, "FTEdit",
	"Do you want to save the changes you made?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
	if (ret == QMessageBox::Cancel)
		return (false);
	else if (ret == QMessageBox::Save)
	{
		save();
		return (!fileManager->getPath().isEmpty() && fileManager->getErrorMessage().isEmpty());
	}
	modified = false;
	return (true);
}

void MainWindow::reset()
{
	if (editor)
		delete editor;
	editor = nullptr;
	modified = false;
	curItem = nullptr;
	qDeleteAll(resultsHistory); // Discard all results
	qDeleteAll(trees->takeChildren());
	qDeleteAll(results->takeChildren());
	errorList->clear();
	fileManager->setPath("");
}

void MainWindow::resizeSplitter(QSplitter *splitter, int widget1Size, int widget2Size)
{
	QList<int> l;

	l << widget1Size << widget2Size;
	splitter->setSizes(l);
	l.clear();
}

void MainWindow::setEnabledButton()
{
	bool isChild = curItem && curItem->isChild();
	bool isNotChild = curItem && !curItem->isChild();
	int pos = 0;
	int size = 0; // children size of parent
	if (curItem && curItem->node()->getParent())
	{
		QList<Node*> &l = curItem->node()->getParent()->getChildren();
		pos = l.indexOf(curItem->node());
		size = l.size() - 1;
	}
	saveAct->setEnabled(!fileManager->getPath().isEmpty() && modified);
	cutAct->setDisabled(curItem == nullptr);
	copyAct->setDisabled(curItem == nullptr);
	pasteAct->setEnabled(editor->getClipboard() && (isNotChild || dynamic_cast<Gate*>(editor->getClipboard())));
	moveItemFirstAct->setEnabled(pos > 0);
	moveItemLeftAct->setEnabled(pos > 0);
	moveItemRightAct->setEnabled(pos < size);
	moveItemLastAct->setEnabled(pos < size);
	editItemAct->setDisabled(curItem == nullptr);
	removeItemAct->setDisabled(curItem == nullptr);
	detachItemAct->setEnabled(isNotChild);
	joinItemAct->setEnabled(isNotChild && editor->getTrees().size() > 1);
	addAndAct->setDisabled(isChild);
	addInhibitAct->setDisabled(isChild);
	addOrAct->setDisabled(isChild);
	addKNAct->setDisabled(isChild);
	addXorAct->setDisabled(isChild);
	addTransfertAct->setEnabled(isNotChild);
	addEventAct->setEnabled(isNotChild);
	evaluateAct->setEnabled(isNotChild);
}

void MainWindow::addGate(Gate *g)
{
	modified = true;
	editor->getGates() << g;
	if (curItem)
	{
		g->attach((Gate*)curItem->node());
		g = (Gate*)curItem->node();
	}
	else
		editor->getSelection()->setTop(g);
	updateScene(g);
}

void MainWindow::updateScene(Node *selection)
{
	scene->clear();
	Node *top = editor->getSelection()->getTop();
	if (top)
	{
		top->balanceNodePos(); // Reset node position
		RenderVisitor visitor(*this, selection);
		top->accept(visitor);
	}
	else // Reset to empty tree
	{
		curItem = nullptr;
		setEnabledButton();
	}
	scene->setSceneRect(scene->itemsBoundingRect());
	view->update();
}

QMenu *MainWindow::itemsContextMenu()
{
	return (itemsMenu);
}

QGraphicsScene *MainWindow::getScene()
{
	return (scene);
}
