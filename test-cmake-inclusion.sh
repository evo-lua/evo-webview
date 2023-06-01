set -e

echo "Testing library consumption via CMake"

cp -r Tests/Fixtures/WebViewTestApp .. 
cd ../WebViewTestApp
echo "OK	Simulated project creation"

mkdir -p webview
cp -r ../evo-webview/* ./webview
echo "OK	Simulated checkout"

cmake -S . -B build
echo "OK	CMake configuration step"

cmake --build build  --config Release
echo "OK	CMake build test app with static webview library"

build/WebViewTestApp
echo "OK	Run compiled test app"

cd -