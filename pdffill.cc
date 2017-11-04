// pdffill [-l] [-F] [-s FIELD=VALUE] SRC [DST] - PDF form fill utility
//
// -l lists all form field
// -F fills all form fields with their names
// -s FIELD=VALUE assigns VALUE to FIELD
// The result is saved to DST, or standard output when DST is '-'.

// Copyright (C) 2017 Leah Neukirchen <purl.org/net/chneukirchen>
// Parts of code derived from pdf-fill-form, which is
// Copyright (c) 2015 Tommi Pisto <https://github.com/tpisto/pdf-fill-form>
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <iostream>
#include <string>

#include <QCommandLineParser>
#include <QMap>
#include <QFile>

#include <poppler/qt5/poppler-form.h>
#include <poppler/qt5/poppler-qt5.h>

void
assign(Poppler::FormField *field, QString value)
{

	if (field->isReadOnly() || !field->isVisible())
		return;

	if (field->type() == Poppler::FormField::FormText) {
		Poppler::FormFieldText *textField = (Poppler::FormFieldText *) field;
		textField->setText(value);
	} else if (field->type() == Poppler::FormField::FormChoice) {
		Poppler::FormFieldChoice *choiceField = (Poppler::FormFieldChoice *) field;
		if (choiceField->isEditable()) {
			choiceField->setEditChoice(value);
		} else {
			QStringList possibleChoices = choiceField->choices();
			int index = possibleChoices.indexOf(value);
			if (index >= 0)
				choiceField->setCurrentChoices(QList<int>{index});
			else
				std::cerr << "can't set " <<
				    field->fullyQualifiedName().toStdString() <<
				    " to " << value.toStdString() << std::endl;
		}
        } else if (field->type() == Poppler::FormField::FormButton) {
		Poppler::FormFieldButton* myButton = (Poppler::FormFieldButton *)field;
		switch (myButton->buttonType()) {
		case Poppler::FormFieldButton::Push:
			break;
		case Poppler::FormFieldButton::CheckBox:
			myButton->setState(value == "true" ||
			    value == "1" ||
			    value == "yes");
			break;
		case Poppler::FormFieldButton::Radio:
			myButton->setState(myButton->caption().toUtf8().constData() == value);
			break;
		}
	}
	// else Signature: ignored
}

QString
content(Poppler::FormField *field)
{

	if (field->type() == Poppler::FormField::FormText) {
		Poppler::FormFieldText *textField = (Poppler::FormFieldText *) field;
		return textField->text();
	} else if (field->type() == Poppler::FormField::FormChoice) {

		Poppler::FormFieldChoice *choiceField = (Poppler::FormFieldChoice *) field;
		if (choiceField->isEditable()) {
			return choiceField->editChoice();
		} else {
			QStringList possibleChoices = choiceField->choices();
			QString r;
			for (int i : choiceField->currentChoices()) {
				if (!r.isEmpty())
					r += ",";
				r += possibleChoices.at(i);
			}
			return r;
		}
        } else if (field->type() == Poppler::FormField::FormButton) {
		Poppler::FormFieldButton* myButton = (Poppler::FormFieldButton *)field;
		switch (myButton->buttonType()) {
		case Poppler::FormFieldButton::Push:
			return "<button>";
		case Poppler::FormFieldButton::CheckBox:
			return myButton->state() ? "true" : "false";
		case Poppler::FormFieldButton::Radio:
			return myButton->state() ? "true" : "false";
		}
	}
	// else Signature: ignored

	return "<unknown-form-element>";
}

int
main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName("pdffill");

	QCommandLineParser parser;
	parser.setApplicationDescription("PDF form fill utility");
	parser.addHelpOption();
	parser.addPositionalArgument("src", "source file");
	parser.addPositionalArgument("dst", "destination file");

	QCommandLineOption listOption("l", "list form fields");
	parser.addOption(listOption);

	QCommandLineOption fillfieldOption("F", "fill form fields with names");
	parser.addOption(fillfieldOption);

	QCommandLineOption setOption("s", "set form field to value", "field=value");
	parser.addOption(setOption);

	parser.process(app);

	const QStringList args = parser.positionalArguments();

	if (args.length() < 1) {
		std::cerr << parser.helpText().toStdString();
		return 1;
	}

	QMap<QString, QString> assgs;
	QMap<QString, bool> assigned;

	if (parser.isSet(setOption)) {
		int eq;
		for (const auto &assg : parser.values(setOption)) {
			if ((eq = assg.indexOf('=')) >= 0) {
				assgs[assg.left(eq)] = assg.mid(eq + 1);
				assigned[assg.left(eq)] = false;
			} else {
				std::cerr << "not an assigment " << assg.toStdString() << std::endl;
			}
		}
	}


	Poppler::Document *document = Poppler::Document::load(args.at(0));
	if (!document)
		return 1;

	int n = document->numPages();

	if (parser.isSet(listOption))
		std::cerr << n << " pages total" << std::endl;

	for (int i = 0; i < n; i++) {
		Poppler::Page *page = document->page(i);

		for (auto *field : page->formFields()) {
			std::string fieldName = field->fullyQualifiedName().toStdString();
			if (parser.isSet(listOption))
				std::cout << "Page " << i+1 << " : " << fieldName
					  << " (" << field->id() << ") = "
					  << content(field).toStdString()
					  << std::endl;
			
			if (parser.isSet(fillfieldOption))
				assign(field, field->fullyQualifiedName());

			if (parser.isSet(setOption)) {
				auto fqn = field->fullyQualifiedName();
				if (assgs.contains(fqn)) {
					assign(field, assgs[fqn]);
					assigned[fqn] = true;
				} else {
					QString sid(QString::number(field->id()));
					if (assgs.contains(sid)) {
						assign(field, assgs[sid]);
						assigned[sid] = true;
					}
				}
			}
		}
	}

	if (parser.isSet(setOption))
		for (auto i = assigned.constBegin(); i != assigned.constEnd(); ++i)
			if (!i.value())
				std::cerr << i.key().toStdString() << " not found!" << std::endl;

	if (args.length() > 1 &&
	    (parser.isSet(setOption) || parser.isSet(fillfieldOption))) {
		Poppler::PDFConverter *converter = document->pdfConverter();
		converter->setPDFOptions(converter->pdfOptions() |
		    Poppler::PDFConverter::WithChanges);
		if (args.at(1) == "-")
			converter->setOutputFileName("/dev/stdout");
		else
			converter->setOutputFileName(args.at(1));

		if (!converter->convert()) {
			std::cerr << "failed to convert.\n";
			return 1;
		}
	}

	return 0;
}
