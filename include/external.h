#ifndef EXTERNAL_H
#define EXTERNAL_H

#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <curl/curl.h>

void find_external_resources(const char *html_content, const char *base_url);

#endif