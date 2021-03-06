#include <xapian.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>

#include <cstdlib> // For exit().
#include <cstring>
#include <sstream>

#define BUFF_LEN 1024

using namespace std;

string
html_escape(const string &str)
{
    string res;
    string::size_type p = 0;
    while (p < str.size()) {
	char ch = str[p++];
	switch (ch) {
	    case '<':
		res += "&lt;";
		continue;
	    case '>':
		res += "&gt;";
		continue;
	    case '&':
		res += "&amp;";
		continue;
	    case '"':
		res += "&quot;";
		continue;
	    default:
		res += ch;
	}
    }
    return res;
}

static int word_in_list(const string& word, const string& list)
{
    string::size_type split = 0, split2;
    int count = 0;
    while ((split2 = list.find('\t', split)) != string::npos) {
	if (word.size() == split2 - split) {
	    if (memcmp(word.data(), list.data() + split, word.size()) == 0)
		return count;
	}
	split = split2 + 1;
	++count;
    }
    if (word.size() == list.size() - split) {
	if (memcmp(word.data(), list.data() + split, word.size()) == 0)
	    return count;
    }
    return -1;
}

static string
html_highlight(const string &s, const string &list,
	       const string &bra, const string &ket)
{
    Xapian::Stem stemmer("english");

    string res;

    using namespace Xapian::Unicode;

    Xapian::Utf8Iterator j(s);
    const Xapian::Utf8Iterator s_end;
    while (true) {
	Xapian::Utf8Iterator first = j;
	while (first != s_end && !is_wordchar(*first)) ++first;
	if (first == s_end) break;
	Xapian::Utf8Iterator term_end;
	string term;
	string word;
	const char *l = j.raw();
	if (*first < 128 && isupper(*first)) {
	    j = first;
	    Xapian::Unicode::append_utf8(term, *j);
	    while (++j != s_end && *j == '.' && ++j != s_end && *j < 128 && isupper(*j)) {
		Xapian::Unicode::append_utf8(term, *j);
	    }
	    if (term.length() < 2 || (j != s_end && is_wordchar(*j))) {
		term.resize(0);
	    }
	    term_end = j;
	}
	if (term.empty()) {
	    j = first;
	    while (is_wordchar(*j)) {
		Xapian::Unicode::append_utf8(term, *j);
		++j;
		if (j == s_end) break;
		if (*j == '&' || *j == '\'') {
		    Xapian::Utf8Iterator next = j;
		    ++next;
		    if (next == s_end || !is_wordchar(*next)) break;
		    term += *j;
		    j = next;
		}
	    }
	    term_end = j;
	    if (j != s_end && (*j == '+' || *j == '-' || *j == '#')) {
		string::size_type len = term.length();
		if (*j == '#') {
		    term += '#';
		    do { ++j; } while (j != s_end && *j == '#');
		} else {
		    while (j != s_end && (*j == '+' || *j == '-')) {
			Xapian::Unicode::append_utf8(term, *j);
			++j;
		    }
		}
		if (term.size() - len > 3 || (j != s_end && is_wordchar(*j))) {
		    term.resize(len);
		} else {
		    term_end = j;
		}
	    }
	}
	j = term_end;
	term = Xapian::Unicode::tolower(term);
	int match = word_in_list(term, list);
	if (match == -1) {
	    string stem = "Z";
	    stem += stemmer(term);
	    match = word_in_list(stem, list);
	}
	if (match >= 0) {
	    res += html_escape(string(l, first.raw() - l));
	    if (!bra.empty()) {
		res += bra;
	    } else {
		static const char * colours[] = {
		    "ffff66", "99ff99", "99ffff", "ff66ff", "ff9999",
		    "990000", "009900", "996600", "006699", "990099"
		};
		size_t idx = match % (sizeof(colours) / sizeof(colours[0]));
		const char * bg = colours[idx];
		if (strchr(bg, 'f')) {
		    res += "<b style=\"color:black;background-color:#";
		} else {
		    res += "<b style=\"color:white;background-color:#";
		}
		res += bg;
		res += "\">";
	    }
	    word = string(first.raw(), j.raw() - first.raw());
	    res += html_escape(word);
	    if (!bra.empty()) {
		res += ket;
	    } else {
		res += "</b>";
	    }
	} else {
	    res += html_escape(string(l, j.raw() - l));
	}
    }
    if (j != s_end) res += html_escape(string(j.raw(), j.left()));
    return res;
}

void
print_snippet(ofstream& html_out, string snippet, string query,
	      int queryno, int snippetno)
{
    string query_termlist;
    Xapian::Stem stemmer("english");

    Xapian::TermGenerator termgen;
    Xapian::Document querydoc;

    termgen.set_document(querydoc);
    termgen.set_stemmer(stemmer);
    termgen.index_text(query);

    for (Xapian::TermIterator it = querydoc.termlist_begin(); it != querydoc.termlist_end(); it++)
	query_termlist += *it + "\t";

    snippet = html_highlight(snippet, query_termlist, "", "");
    html_out << snippet << endl;
    html_out << " <br/> " << endl;
    string radio_str1("<input type=\"radio\" name=\"");
    string radio_str2("\" value=\"");
    string radio_str3("\"/> ");
    string radio_str3_checked("\" checked/> ");
    html_out << radio_str1 << queryno << "_" << snippetno
	 << radio_str2 << 1 << radio_str3 << 1;
    html_out << radio_str1 << queryno << "_" << snippetno
	 << radio_str2 << 2 << radio_str3 << 2;
    html_out << radio_str1 << queryno << "_" << snippetno
	 << radio_str2 << 3 << radio_str3_checked << 3;
    html_out << radio_str1 << queryno << "_" << snippetno
	 << radio_str2 << 4 << radio_str3 << 4;
    html_out << radio_str1 << queryno << "_" << snippetno
	 << radio_str2 << 5 << radio_str3 << 5;
    html_out << " <br/> " << endl;
    html_out << " <br/> " << endl;
}

void
test_file(
	const char *		    filename,
	Xapian::QueryParser &	    qp,
	Xapian::Enquire &	    enquire,
	vector<string>&		    data,
	vector<string>&		    form_ids,
	ofstream&		    html_out)
{
    ifstream file(filename);
    char buff[BUFF_LEN];

    // Get the ground truth snippets for each url.
    int queryno = 0;
    while (!file.eof()) {
	file.getline(buff, BUFF_LEN);
	string query_s(buff);
	html_out << "<h3> Query:" << query_s << "</h3>";

	Xapian::Query query = qp.parse_query(query_s);
	enquire.set_query(query);
	// Find the top 15 results for the query.
	Xapian::MSet matches = enquire.get_mset(0, 10);

	Xapian::Snipper snipper;
	Xapian::Stem stemmer("english");
	snipper.set_stemmer(stemmer);
	snipper.set_mset(matches);

	// Display the results.
	for (Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i) {
	    // Get only the sample from the document data.
	    string gen_text = i.get_document().get_data();
	    // Get url name
	    string url = gen_text.substr(5, gen_text.find('\n') - 5);
	    string sample_mark("sample=");
	    string type_mark("type=");
	    queryno++;

	    size_t sample_pos = gen_text.find(sample_mark) + sample_mark.size();
	    gen_text.erase(gen_text.begin(), gen_text.begin() + sample_pos);

	    size_t type_pos = gen_text.rfind(type_mark);
	    gen_text.erase(gen_text.begin() + type_pos, gen_text.end());

	    html_out << "<a href=http://en.wikipedia.org/wiki/" << url
		 << "> <b> From:" << url << "</b> </a> <br/> " << endl;

	    int snippetno = 0;
	    int dn[6] =	    {10,  5,  5,  5, 10, 10};
	    int ws[6] =	    {10, 30, 10, 20, 40, 20};
	    double sc[6] =  {.5, .5, .9, .5, .5, .9};

	    for (unsigned int i = 0; i < 6; i++) {
		snipper.set_rm_docno(dn[i]);
		snipper.set_mset(matches);
		snipper.set_smoothing_coef(sc[i]);
		snipper.set_window_size(ws[i]);
		data.push_back(query_s + "\\n" + url);
		stringstream id;
		id << queryno << "_" << ++snippetno;
		form_ids.push_back(id.str());

		string snippet = snipper.generate_snippet(gen_text);
		print_snippet(html_out, snippet, query_s, queryno, snippetno);
	    }
	}
    }
    file.close();
}

void
print_php_script(ofstream& php_out, const vector<string>& data,
		 const vector<string>& form_ids)
{
    php_out << "<?php" << endl;
    php_out << "if(isset($_POST[\"submit\"])) {" << endl;

    // Open file.
    php_out << "\t$filename = $_POST[\"username\"] . \".txt\";" << endl;
    php_out << "\tif (ctype_alnum($_POST[\"username\"]) == FALSE) {\n"
	       "\t\techo \"Enter alphanumeric username!\";\n"
	       "\t\texit();\n"
	       "\t}\n";
    php_out << "\tif (@stat($filename) != FALSE) {\n"
	       "\t\techo \"Username taken, Try other username!\";\n"
	       "\t\texit();\n"
	       "\t}\n";
    php_out << "\t$file = fopen($filename, 'w') or die(\"Error\");" << endl;

    // Write data.
    for (unsigned int i = 0; i < data.size(); i++) {
	php_out << "\t$info = $_POST[\"" << form_ids[i] << "\"];" << endl;

	string doc_info = data[i] + "\\n" + form_ids[i] + "\\n";
	php_out << "\tfwrite($file,\"" << doc_info << "\");" << endl;
	php_out << "\t$info = $info . \"\\n\";" << endl;
	php_out << "\tfwrite($file, $info);" << endl;
    }
    php_out << "\tfclose($file);" << endl;
    php_out << "echo \"Thanks!\";" << endl;
    php_out << "}" << endl;
    php_out << "?>";
}

void
print_html_bra(ofstream& html_out, const char* php_file)
{
    html_out << "<!DOCTYPE html>"
	    "<html>"
	    "<head>"
	    "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">"
	    "<title>Snippet assessor</title>"
	    "</head>"
	    "<body>";
    html_out << "<form action=\"" << php_file << "\" method=\"post\">";
    html_out << "<h3> Name </h3>";
    html_out << "<input type=\"text\" name=\"username\">";
}

void
print_html_ket(ofstream& html_out)
{
    html_out << "<input type=\"submit\" name=\"submit\" value=\"submit\">"
	    "</form>";
    html_out << "</body> </html>";
}

int
main(int argc, char **argv)
{
    try {
        // We require at least 5 command line arguments.
        if (argc < 5) {
	    int rc = 1;
	    if (argv[1]) {
		if (strcmp(argv[1], "--version") == 0) {
		    cout << "evalgen" << endl;
		    exit(0);
		}
		if (strcmp(argv[1], "--help") == 0) {
		    rc = 0;
		}
	    }
	    cout << "Usage: " << argv[0] << " PATH_TO_DATABASE QUERYFILE HTML_FILE PHP_FILE" << endl;
	    exit(rc);
	}

	// Open the database for searching.
	Xapian::Database db(argv[1]);

	// Start an enquire session.
	Xapian::Enquire enquire(db);

	ofstream html_out(argv[3]);
	ofstream php_out(argv[4]);

	// Set query parser.
	Xapian::QueryParser qp;
	Xapian::Stem stemmer("english");
	qp.set_stemmer(stemmer);
	qp.set_database(db);
	qp.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);
	vector<string> data;
	vector<string> form_ids;
	print_html_bra(html_out, argv[4]);
	test_file(argv[2], qp, enquire, data, form_ids, html_out);
	print_html_ket(html_out);
	print_php_script(php_out, data, form_ids);
    } catch (const Xapian::Error &e) {
        cout << e.get_description() << endl;
        exit(1);
    }
}
