Compile your Java/Kotlin/C/C++ Files to android apk!

How it works

it looks for d8
it looks for a JVM
it looks for apksigner
it looks for a manifest.xml in your project directory
if it finds all it would compile properly...apksigner optional

Project Structure

myapp/
├── manifest.xml
├── src/
│   └── com/example/myapp/MainActivity.java
└── res/         (optional)

What your manifest.xml has to look like (or similar)

<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.example.myapp"
    android:versionCode="1"
    android:versionName="1.0">

    <uses-sdk android:minSdkVersion="21" android:targetSdkVersion="34" />

    <application android:label="MyApp">
        <activity android:name=".MainActivity" android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>

Install some crucial things

apt install ecj d8 aapt2 apksigner

git clone https://github.com/Sekid401/compiledroid
cd compiledroid
make && make install
  
Good luck!

Tip: run make && make install to install

Make sure android.jar is in $PREFIX/share/compiledroid

mkdir -p $PREFIX/share/compiledroid/sdk
curl -L -o $PREFIX/share/compiledroid/sdk/android.jar \
  https://github.com/Sable/android-platforms/raw/master/android-34/android.jar
