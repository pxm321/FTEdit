#include <limits>
#include "Dialog.hh"

void ChooseDistributionDialog::confirm()
{
	QString name = editor.generateName("Model");
	Distribution *dist;
	if (laws->checkedId() == 1)
		dist = new Constant(name);
	else if (laws->checkedId() == 2)
		dist = new Exponential(name);
	else
		dist = new Weibull(name);
	editor.getDistributions() << dist; // Append new distribution to list
	accept();
}

ChooseDistributionDialog::ChooseDistributionDialog(QWidget *parent, Editor &editor)
: QDialog(parent), editor(editor)
{
	setWindowTitle("New model");
	setWindowIcon(QIcon(":icons/add.png"));
	auto layout = new QVBoxLayout(this);
	WidgetLinker linker(this, layout);
	linker.addLayoutItem(new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Maximum));
	auto hLayout = new QHBoxLayout();
	linker.set(hLayout);
	linker.addLayoutItem(new QSpacerItem(60, 0, QSizePolicy::Maximum, QSizePolicy::Minimum));
	linker.set(new QVBoxLayout());
	linker.addLabel("Law of probability:");
	laws = new QButtonGroup(this);
	auto button = linker.addRadioButton("Constant");
	button->setChecked(true);
	laws->addButton(button, 1);
	laws->addButton(linker.addRadioButton("Exponential"), 2);
	laws->addButton(linker.addRadioButton("Weibull"), 3);
	linker.set(hLayout);
	linker.addLayoutItem(new QSpacerItem(60, 0, QSizePolicy::Maximum, QSizePolicy::Minimum));
	linker.set(layout);

	connect(linker.addOKButton(false), SIGNAL(released()), this, SLOT(confirm()));
}

PropertiesDialog::PropertiesDialog(QWidget *parent, Editor &editor, Properties *prop, bool initialized) :
QDialog(parent), prop(prop), isValid(true), editor(editor)
{
	if (!initialized)
		return ;
	init();
}

void PropertiesDialog::closeEvent(QCloseEvent *event)
{
	isValid ? event->accept() : event->ignore();
}

void PropertiesDialog::checkName(const QString &text)
{
	QString s = text.trimmed();
	if (!s.compare(prop->getName()) || (s.size() > 0 && editor.isUnique(s)))
	{
		isValid = true;
		nameWidget->setToolTip("");
		nameWidget->setStyleSheet("");
		prop->setName(s);
	}
	else
	{
		isValid = false;
		nameWidget->setToolTip(s.size() ? "This name already exists" : "This name is invalid");
		nameWidget->setStyleSheet("border: 1px solid red;background-color: #ff8a8a;");
	}
	emit nameChanged(s); // Signal
}

void PropertiesDialog::editDesc()
{
	prop->setDesc(descWidget->toPlainText());
}

void PropertiesDialog::init()
{
	if (!this->layout())
		new QVBoxLayout(this);
	setWindowIcon(QIcon(":icons/edit.png"));
	WidgetLinker linker(this, (QBoxLayout*)this->layout());
	linker.addLabel("Name:");
	nameWidget = linker.addLineEdit(prop->getName());
	linker.addLabel("Description:");
	descWidget = linker.addTextEdit(prop->getDesc());
	connect(nameWidget, SIGNAL(textEdited(const QString &)), this, SLOT(checkName(const QString &)));
	connect(descWidget, SIGNAL(textChanged()), this, SLOT(editDesc()));
}

void PropertiesDialog::setProperties(Properties *prop)
{
	this->prop = prop;
	nameWidget->blockSignals(true);
	nameWidget->setText(prop->getName());
	nameWidget->blockSignals(false);
	descWidget->blockSignals(true);
	descWidget->setText(prop->getDesc());
	descWidget->blockSignals(false);
}

bool PropertiesDialog::valid()
{
	return (isValid);
}

void ChooseResultDialog::confirm()
{
	if (useBoolean->isChecked())
	{
		QString s;
		if (missionTime->value() > 0.0 && step->value() == 0.0)
			s = "The step must be positive.";
		else if (missionTime->value() / step->value() > 1e6)
			s = "Limit exceeded.";
		if (!s.isEmpty())
		{
			QMessageBox msg(this);
			msg.setIcon(QMessageBox::Critical);
			msg.setWindowTitle("Error");
			msg.setText(s);
			msg.exec();
			return ;
		}
	}
	results << new Result(top, useMCS->isChecked(), useBoolean->isChecked(),
	missionTime->value(), step->value()); // Add new analysis to the list
	accept();
}

void ChooseResultDialog::checkChanged(int state)
{
	(void)state;
	missionTime->setEnabled(useBoolean->isChecked() || useMCS->isChecked());
	step->setEnabled(useBoolean->isChecked());
}

ChooseResultDialog::ChooseResultDialog(QWidget *parent, Gate *top, QList<Result*> &results)
: QDialog(parent), top(top), results(results)
{
	setWindowTitle("Fault tree analysis");
	setWindowIcon(QIcon(":icons/evaluate.png"));
	auto layout = new QVBoxLayout(this);
	WidgetLinker linker(this, layout);
	linker.addLayoutItem(new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Maximum));
	auto *button = linker.addCheckBox("Perform basic check");
	button->setToolTip("Check if the tree logic is correct");
	button->setChecked(true);
	button->setEnabled(false);
	useBoolean = linker.addCheckBox("Use boolean algebra");
	useBoolean->setToolTip("Use Boolean algebra to calculate the probabilities of failure of the top event");
	useMCS = linker.addCheckBox("Find MCS");
	useMCS->setToolTip("Calculate the minimal cuts sets and their probability of failure with MOCUS");
	linker.addLabel("Mission time :")->setToolTip("Maximum operating time");;
	missionTime = linker.addDoubleSpinBox();
	missionTime->setSuffix(" s");
	missionTime->setRange(0, std::numeric_limits<double>::max());
	missionTime->setValue(0);
	missionTime->setEnabled(false);
	linker.addLabel("Step :")->setToolTip("Time between each calculation");
	step = linker.addDoubleSpinBox();
	step->setSuffix(" s");
	step->setRange(0, std::numeric_limits<double>::max());
	missionTime->setValue(0);
	step->setEnabled(false);
	auto *button2 = linker.addOKButton(false);
	button2->setText("Continue");
	resize(340, height());

	connect(useBoolean, SIGNAL(stateChanged(int)), this, SLOT(checkChanged(int)));
	connect(useMCS, SIGNAL(stateChanged(int)), this, SLOT(checkChanged(int)));
	connect(button2, SIGNAL(released()), this, SLOT(confirm()));
}

ChooseTreeDialog::ChooseTreeDialog(QWidget *parent, Editor &editor, int &treeIndex) :
QDialog(parent), editor(editor), treeIndex(treeIndex)
{
	setWindowTitle("Merge fault tree");
	setWindowIcon(QIcon(":icons/add.png"));
	WidgetLinker linker(this, new QVBoxLayout(this));
	linker.addLayoutItem(new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Maximum));
	trees = linker.addComboBox();
	connect(linker.addOKButton(false), SIGNAL(released()), this, SLOT(accept()));

	QList<Tree> &l = editor.getTrees();
	QString current = editor.getSelection()->getProperties().getName();
	int i = 0;
	for (auto &tree : l)
	{
		trees->addItem(tree.getProperties().getName());
		if (!tree.getProperties().getName().compare(current))
		{
			auto model = qobject_cast<QStandardItemModel *>(trees->model());
			i = model->rowCount() - 1;
			QStandardItem *item = model->item(i);
			item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
		}
	}

	trees->setCurrentIndex(i < 1);
	treeIndex = trees->currentIndex();
	connect(trees, QOverload<int>::of(&QComboBox::currentIndexChanged),
	[=](int i) { this->treeIndex = i; });
}
