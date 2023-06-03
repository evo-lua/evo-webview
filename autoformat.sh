echo "Discovering sources ..."

RELEVANT_FILES_TO_FORMAT=$(find . -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" -o -name "*.mm" -o -name "*.cc" \) -print -o -path "*/deps" -prune -o -path "*/cmakebuild*" -prune)

if [ -n "$RELEVANT_FILES_TO_FORMAT" ]; then
	echo "Discovered sources:"
	echo $RELEVANT_FILES_TO_FORMAT

	echo "Formatting sources ..."
	clang-format -i --verbose $RELEVANT_FILES_TO_FORMAT
else
	echo "NO relevant sources found"
fi
