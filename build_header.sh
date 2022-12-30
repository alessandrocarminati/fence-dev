#!/bin/sh
echo "const char  home_html[]={"
cat  htmls/home.html | gzip |  xxd -i
echo "};"
echo "const char  style_css[]={"
cat  htmls/style.css | gzip |  xxd -i
echo "};"
echo "const char  setup_html[]={"
cat  htmls/setup.html | gzip |  xxd -i
echo "};"
echo "const char  wsconsole_html[]={"
cat htmls/wsconsole.html | gzip |  xxd -i
echo "};"
echo "const char  on_icon[]={"
cat imgs/on.gif | xxd -i
echo "};"
echo "const char  off_icon[]={"
cat imgs/off.gif | xxd -i
echo "};"
