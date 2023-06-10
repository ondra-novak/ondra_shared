#ifndef _ONDRA_SHARED_INI_PARSER_23123148209810_
#define _ONDRA_SHARED_INI_PARSER_23123148209810_

#include <cctype>
#include <string_view>
#include <vector>


namespace ondra_shared {


struct IniItem {

     enum Type {
          comment,
          directive,
          data
     };

     Type type;
     std::string_view section;
     std::string_view key;
     std::string_view value;

     IniItem(Type t, const std::string_view &a):type(t),value(a) {}
     IniItem(Type t, const std::string_view &a, const std::string_view &b):type(t),key(a),value(b) {}
     IniItem(Type t, const std::string_view &a, const std::string_view &b, const std::string_view &c):type(t),section(a),key(b),value(c) {}


};

///Parses ini and generates list of items
/**
 * The Output function has following prototype
 *
 * void(IniItem);
 *
 * The ini file is defined as text file separated to sections where each section
 * is collection of key=value. The sections are one-level hierarchy
 *
 * @code
 * [section]
 * key=value
 * another key=another value
 *
 * [another section]
 * key=value
 * @endcode
 *
 * The section can contain any characters excluding new line characters and character ]
 * The key can contain any characters excluding new line character and character '='
 * The value can contain any characters excluding new line character. You can use
 *   backslash character to escape various characters. Putting \ at the end of line
 *   causes that new line character is skipped, so reading continues on next line. You
 *   can split large value to multiple lines. You can also use \r and \n to
 *   put new line character to the value. Escape any other character causes that character
 *   is copied to the value. You can escape \\ to write single \
 *
 *   (under windows, the path separator is /, internally converted to \)
 *
 * There can be also directives
 * The directive starts with @ followed by name of directive
 *
 * @code
 *   @include security.ini
 *
 *   [database]
 *   server=localhost
 *   port=3306
 *   ....
 *
 * @endcode
 *
 * The comments starts by #. They are allowed only alone at whole line
 *
 * @code
 * # comment 1 line
 * # commend 2 line
 *
 * key=value # this is not the comment, the content is part of the value
 *
 * @endcode
 *
 *
 * @b special @b directives
 *
 * @\ - changes escape character
 *
 * @code
 * @\ ^
 * [section]
 * key=Now the escape character ^^, You can put new line ^n, The value can be ^
 *      separated into multiple lines
 * @endif
 *
 *
 */
template<typename Output>
class IniParser {
     enum State {
          beginLine,comment,section,key,value,valueEscaped,
          valueEscapedNl,waitForEoln,directiveKeyword,directiveData
     };


public:


     IniParser(Output &&out)
          :buffer(1,0)
          ,sectionSize(1)
          ,keySize(0)
          ,valueSize(0)
          ,escapeChar('\\')
          ,fn(std::forward<Output>(out))
          ,curState(beginLine)
          ,afterEscapeState(beginLine){}


     void operator()(int c) {

          switch (curState) {
          case beginLine: readBeginLine(c);break;
          case comment: readComment(c);break;
          case section: readSection(c);break;
          case key: readKey(c);break;
          case value: readValue(c);break;
          case valueEscaped: readValueEscaped(c);break;
          case valueEscapedNl: readValueEscapedNl(c);break;
          case waitForEoln: waitTillEoln(c);break;
          case directiveKeyword: readDirectiveKeyword(c);break;
          case directiveData: readDirectiveData(c);break;
          }

     }


protected:
     std::vector<char> buffer;
     std::size_t sectionSize;
     std::size_t keySize;
     std::size_t valueSize;
     char escapeChar;

     Output fn;

     State curState;
     State afterEscapeState;

     static bool isnl(int c) {
          return c == '\n' || c == '\r';
     }


     void readBeginLine(int c) {
          if (std::isspace(c)) return;



          switch (c) {
          case '#':  curState = comment;
                       buffer.resize(sectionSize);
                       break;
          case '[': curState = section;
                      buffer.clear();
                      break;
          case '@': curState = directiveKeyword;
                         buffer.resize(sectionSize);
                         break;
          default: curState = key;
                    buffer.resize(sectionSize);
                    readKey(c);
                         break;
          }
     }

     void readComment(int c) {
          if (isnl(c)) {
               curState = beginLine;
               fn(IniItem(IniItem::comment, std::string_view(buffer.data()+sectionSize, buffer.size()-sectionSize)));
          } else {
               buffer.push_back((char)c);
          }
     }

     void readSection(int c) {

          if (c ==']') {
               buffer.push_back(0);
               sectionSize = buffer.size();
               curState = waitForEoln;
          } else {
               buffer.push_back((char)c);
          }
     }

     void readKey(int c) {
          if (c == '=') {
               buffer.push_back(0);
               keySize = buffer.size() - sectionSize;
               curState = value;
          } else if (isnl(c)) {
               buffer.push_back(0);
               keySize = buffer.size() - sectionSize;
               readValue(c);
          } else {
               buffer.push_back((char)c);
          }
     }

     void readValue(int c) {
          readValueGen(c, value, IniItem::data);
     }

     static std::string_view trim(std::string_view what) {
         while (!what.empty() && std::isspace(what.front())) {
             what = what.substr(1);
         }
         while (!what.empty() && std::isspace(what.back())) {
             what = what.substr(0, what.length()-1);
         }
         return what;
     }

     void readValueGen(int c, State escState, IniItem::Type dataType) {
          if (isnl(c)) {
               buffer.push_back(0);
               valueSize = buffer.size() - (keySize+sectionSize);
               std::string_view sectionName(buffer.data(), sectionSize-1);
               std::string_view keyName(buffer.data()+sectionSize, keySize-1);
               std::string_view valueName(buffer.data()+sectionSize+keySize, valueSize-1);
               sectionName=trim(sectionName);
               keyName=trim(keyName);
               valueName=trim(valueName);
               buffer[sectionName.data()+sectionName.length() - buffer.data()] = 0;
               buffer[keyName.data()+keyName.length() - buffer.data()] = 0;
               buffer[valueName.data()+valueName.length() - buffer.data()] = 0;
               bool processed = false;
               if (dataType == IniItem::directive) {
                    if (keyName == "\\") {
                         if (valueName.length() == 1) {
                              escapeChar = valueName[0];
                              processed = true;
                         }
                    }
               }
               if (!processed) {
                    fn(IniItem(dataType,sectionName, keyName, valueName));
               }
               curState = beginLine;
          } else if (c == escapeChar) {
               afterEscapeState = escState;
               curState = valueEscaped;
          } else {
               buffer.push_back((char)c);
          }
     }

     void readValueEscaped(int c) {
          switch (c) {
          case '\n':
          case '\r': curState = valueEscapedNl; return;
          case 'r': buffer.push_back('\r');break;
          case 'n': buffer.push_back('\n');break;
          default: buffer.push_back((char)c);break;
          }
          curState = afterEscapeState;
     }

     void readValueEscapedNl(int c) {
          if (!isspace(c)) {
               curState = afterEscapeState;
               readValue(c);
          }
     }

     void waitTillEoln(int c) {
          if (isnl(c)) {
               curState = beginLine;
          }
     }

     void readDirectiveKeyword(int c) {
          if (isspace(c)) {
               buffer.push_back(0);
               keySize = buffer.size() - sectionSize;
               curState = directiveData;
               if (isnl(c)) readDirectiveData(c);
          } else {
               buffer.push_back((char)c);
          }
     }

     void readDirectiveData(int c) {
          readValueGen(c, directiveData, IniItem::directive);
     }

};




}


#endif
