Telegram-Purple Unofficial (Beta 3)
===================================

Telegram-purple is a Libpurple protocol plugin that adds support for the Telegram messenger.

I keep getting many questions about this plugin in my E-mail, so I've created a
[telegram group chat](https://telegram.me/joinchat/01fb53f301b67d3c7a5532908dfa9a89) for
telegram-purple related discussions or questions.

Beta 3
------

#### 1.2.1

##### Bugfixes 

    - Fix buggy group chat handling
      * Assure that foreign users are always listed by their full name instead of the user ID
      * Don't automatically add group chats to buddy list to prevent incompatibillities with some libpurple clients
      * Fix issue that caused the buddy list to not be correctly updated on joining / leaving participants
      * Fix an issue that caused the user list to be printed on every group chat message in Finch
    
    - Fetch chat information lazily to reduce traffic and prevent flood errors
    - Improve input validation for phone numbers

###### Features

    - Support leaving chats
    - Implement the roomlist-API to allow listing of all active chats

#### 1.2.0

##### Bugfixes

    - Fix error in incoming message handling that caused segmentation faults
    - Fix failing audio and video transfers
    - Improved buddy list handling (should avoid "unknown" contacts popping up)
    - Many small issues (see commit history)
    - Fix issues that caused audio and video downloads to not have a proper file ending
    - Fix issue that caused very old unused chats to not be available

###### Features

    - Support creation of new group chats
    - Support joining chats by invite link
    - Support exporting chats by invite link
    - Support displaying and sending GIFs 
    - Mark messages read on other devices as 'deferred' to avoid needless notifications


This version should fix the crashes that seem to have occured in many of the older versions. Consider this another intermediate Beta version, as there will be at least another one until I consider issuing a release candidate.

If you migrate from an older version, the Telegram authentication files have been moved to .purple/telegram-purple. If you want to keep your old secret chats, please copy them manually from ~/.telegram-purple, or otherwise you will be prompted for a new SMS code.

Build
-----

Below, you will find the instructions for how to build the libpurple protocol plugin. If you just want to use the plugin in Adium, [download the precompiled packages here.](https://github.com/majn/telegram-purple/releases).

#### 1. Clone

This repository has submodules, so you need to clone recursively.


        git clone --recursive https://github.com/majn/telegram-purple
        cd telegram-purple


#### 2. Fetch all dependencies

##### Fedora

        sudo dnf install gcc openssl-devel glib2-devel libpurple-devel libwebp-devel

And the development files for gcrypt, probably `gcrypt-devel` or something.


###### Debian / Ubuntu

        sudo apt-get install libgcrypt20-dev libssl-dev libglib2.0-dev libpurple-dev libwebp-dev


###### OpenSUSE

        sudo zypper install gcc glib glib-devel libpurple libpurple-devel zlib-devel openssl libopenssl-devel libwebp-devel

And the development files for gcrypt, probably `gcrypt-devel` or something.


#### 3. Compile and install

If libwebp is not available, you can disable sticker support by calling ./configure --disable-libweb instead.

        ./configure
        make
        sudo make install


Pulling Updates
---------------

This repository contains submodules, and a simple pull just won't be enough to update all the submodules’ files. Pull new changes with:


        git pull
        git submodule update --recursive


Usage
-----

#### First Login

The username is your current phone number, including your full country prefix. For Germany, this would be '+49', resulting in a user name like '+49151123456'. Telegram will verify your phone number by sending you a code via sms. You will be prompted for this code, once that happens.

#### Buddy List 

Like in the official Telegram apps, the displayed buddy list consists of all active conversations.

##### Foreign Users

Foreign users, like for example people you only know through a group chat but never had any conversation with, will not be part of your buddy list by default. You can add those users to the buddy list by clicking on "Add contact..." in the users context menu.

#### Using secret chats

You can use Telegram secret chats with this plugin, they will show up as a new buddy with a '!' in front of the buddy name.

One caveat of secret chats in Telegram is that they can only have one endpoint, this is a limitation of the protocol. This means that if you create a secret chat in Pidgin you will not be able to use that chat on your phone. You will be asked whether to accept each secret chat, so you can always choose to accept the chat on a different device if you want. You can set a default behavior for dealing with secret chats (Accept or Decline) in the account settings, if you don't want that prompt to appear every time.

Self destructive messages will be ignored, since I don't know any way to delete them from the conversation and the history.

##### Confirming the key authenticity

Click on the buddy in the buddy list and click on "Show Info" to visualize the key fingerprint.  

##### Initiate secret chats

To initiate a secret chat from Pidgin, click on a Buddy in the Buddy List and hit "Start Secret Chat"

##### Deleting secret chats

If you delete a secret chat from the buddy list, it will be terminated and no longer be usable.


#### Unicode Emojis for Pidgin

The Telegram phone applications for iOS and Android make use of standardized Unicode smileys (called [Emojis](https://en.wikipedia.org/wiki/Emoji)). Pidgin
does not display those smileys natively, but you can install a custom smiley theme like (https://github.com/stv0g/unicode-emoji) or (https://github.com/VxJasonxV/emoji-for-pidgin) and activate it under Settings > Themes > Smiley Theme.


Building the Adium Plugin
-------------------------

Compiling with XCode is a little bit problematic, since it requires you to compile Adium first to get the necessary framework files. My advice is to just use the [prebuilt bundle](https://github.com/majn/telegram-purple/releases), but if you really want to do it, follow these steps:

1. Get the Adium source, compile it with XCode and copy the build output into telegram-adium/Frameworks/Adium. It should contain at least Adium.framework, AdiumLibpurple.framework and AIUitilies.framework
2. Open the Adium source code, go to ./Frameworks and copy libglib.framework and libpurple.framework into telegram-adium/Frameworks/Adium
3. Build the tgl submodule and delete libtgl.so from libs/ (it should only contain libtgl.a)
4. Install libwebp with homebrew, and copy it into your project:

      brew install webp
      cp /usr/local/Cellar/webp/0.4.3/lib/libwebp.a ./telegram-adium/Frameworks/

5. Build the XCode-Project and execute the created bundle


Discussion / Help
-----------------

#### Custom pubkeys

As we want to avoid OpenSSL, it has become necessary to replace the PEM file format. This means that if you use a custom pubkey (which you really REALLY shouldn't be doing), you have to adapt, sorry.

We no longer ship `tg-server.pub` (old format), but instead `tg-server.tlgpub` (new format). If you have a `.pub` and want to continue using telegram-purple, please use this (hopefully highly portable) tool: [pem2bignum](https://github.com/BenWiederhake/pem2bignum)

You can also write your own conversion tool if you prefer. The format is really simple:

1. `e`, the public exponent, encoded as big endian 32 bit fixed length (e.g. `0x00 01 00 01` for 65537)
2. `n_len`, the length of `n` in bytes, encoded as big endian 32 bit fixed length (e.g. `0x00 00 01 00` for a 2048-bit = 256-byte key)
3. `n_raw`, the raw modulus, encoded as big endian, using the previously indicated length (e.g. `0xC1 50 02 3E [248 bytes omitted] 21 79 25 1F` in the case of telegram's public RSA key.)

If you are interested in developing a non-OpenSSL-licensed converter, look into [insane-triangle-banana](https://github.com/BenWiederhake/insane-triangle-banana).

#### Group chat

Telegram group chat for telegram-purple or libtgl related discussions or questions:

    - https://telegram.me/joinchat/01fb53f301b67d3c7a5532908dfa9a89


Authors
-------

Telegram-Purple was written by:

    - Matthias Jentsch <mtthsjntsch@gmail.com>
    - Vitaly Valtman
    - Christopher Althaus <althaus.christopher@gmail.com>
    - Markus Endres <endresma45241@th-nuernberg.de>


Acknowledgements
----------------

This software is based on the library [Libtgl](https://github.com/vysheng/tgl), which was written by Vitaly Valtman <mail@vysheng.ru> and others, see (http://github.com/vysheng/tgl)

For PNG rendering, it includes the [lodepng library](http://lodev.org/lodepng/).

This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/)

This product includes cryptographic software written by Eric Young (eay@cryptsoft.com)

