#include <zypp/Url.h>
#include <stdexcept>
#include <iostream>

int main(void)
{
  std::string str, out;

  str = "http://:@localhost/";
  std::cout << "STR: " << str << std::endl;
  out = zypp::Url(str).toString();
  std::cout << "URL: " << out << std::endl << std::endl;

  str = "mailto:feedback@suse.de?subject=hello";
  std::cout << "STR: " << str << std::endl;
  out = zypp::Url(str).toString();
  std::cout << "URL: " << out << std::endl << std::endl;

  str = "nfs:///foo/bar/trala";
  std::cout << "STR: " << str << std::endl;
  out = zypp::Url(str).toString();
  std::cout << "URL: " << out << std::endl << std::endl;

  str = "ldap://example.net/dc=example,dc=net?cn,sn?sub?(cn=*)";
  std::cout << "STR: " << str << std::endl;
  out = zypp::Url(str).toString();
  std::cout << "URL: " << out << std::endl << std::endl;

  try
  {
    str = "ldap://example.net/dc=example,dc=net?cn,sn?sub?(cn=*)#x";
    std::cout << "STR: " << str << std::endl;
    out = zypp::Url(str).toString();
    std::cout << "URL: " << out << std::endl;
  }
  catch(const std::invalid_argument &e)
  {
    std::cout << "ERR: " << e.what() << std::endl << std::endl;
  }

	return 0;
}

// vim: set ts=2 sts=2 sw=2 ai et: