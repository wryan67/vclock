#include "jumplist.h"

#include <QtGlobal>

#if defined(Q_OS_WIN)

#include <QCoreApplication>
#include <QDir>
#include <QString>

#include <windows.h>
// Must precede the headers that declare the GUIDs, and defines them in this
// file rather than leaving them to be found in an import library.  MinGW's
// copies of those libraries do not agree with MSVC's about which of these they
// carry, and one self-contained translation unit is not worth a link-time
// hunt.
#include <initguid.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

namespace {

// Windows groups taskbar buttons, and attaches jump lists, by AppUserModelID.
// vclock deliberately does not set one: a program that keeps quiet gets an ID
// the shell works out from its executable path, and a shortcut pointing at
// that executable gets the same one worked out the same way.  They therefore
// already match, and the pinned button is already this program's button.
// Setting an explicit ID would break that -- the shortcut would have to carry
// the ID too, as a property written into the .lnk, which the installer would
// then have to write and nothing in NSIS does.

// One entry on the menu: this same program, started with these arguments.
IShellLinkW *makeTask(const QString &arguments, const QString &title)
{
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&link))))
        return nullptr;

    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString dir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    link->SetPath(reinterpret_cast<const wchar_t *>(exe.utf16()));
    link->SetArguments(reinterpret_cast<const wchar_t *>(arguments.utf16()));
    link->SetWorkingDirectory(reinterpret_cast<const wchar_t *>(dir.utf16()));
    // The program draws its own icon at runtime, but the menu is drawn before
    // it runs; the executable's embedded icon is the one Windows already shows
    // for it everywhere else.
    link->SetIconLocation(reinterpret_cast<const wchar_t *>(exe.utf16()), 0);

    // The text on the menu is a property, not a field on the link.  A link
    // with no title is dropped from the list without complaint, so this is the
    // one step here that is not optional.
    IPropertyStore *store = nullptr;
    if (FAILED(link->QueryInterface(IID_PPV_ARGS(&store)))) {
        link->Release();
        return nullptr;
    }
    PROPVARIANT value;
    HRESULT hr = InitPropVariantFromString(
        reinterpret_cast<const wchar_t *>(title.utf16()), &value);
    if (SUCCEEDED(hr)) {
        hr = store->SetValue(PKEY_Title, value);
        PropVariantClear(&value);
    }
    if (SUCCEEDED(hr))
        hr = store->Commit();
    store->Release();
    if (FAILED(hr)) {
        link->Release();
        return nullptr;
    }
    return link;
}

}  // namespace

namespace jumplist {

void install()
{
    // Qt's Windows platform plugin has already initialised COM on this thread,
    // so this normally returns S_FALSE; it is called anyway so that the code
    // does not depend on that, and uninitialised only when it was this call
    // that did the initialising.
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool owned = SUCCEEDED(init) && init != S_FALSE;

    ICustomDestinationList *list = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&list)))) {
        // BeginList hands back the entries the user has removed by hand, which
        // must not be put back; there are none to put back here, so the value
        // is only released.  It cannot be skipped -- CommitList fails without
        // a list begun.
        // Not named "slots": Qt defines that as a keyword for moc, and this
        // file includes enough of Qt to pick the macro up.
        UINT visibleSlots = 0;
        IObjectArray *removed = nullptr;
        if (SUCCEEDED(list->BeginList(&visibleSlots, IID_PPV_ARGS(&removed)))) {
            IObjectCollection *tasks = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                                           CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tasks)))) {
                if (IShellLinkW *task = makeTask(QStringLiteral("--manage"),
                                                 QStringLiteral("Manage clocks"))) {
                    tasks->AddObject(task);
                    task->Release();
                }
                if (IShellLinkW *task = makeTask(QStringLiteral("--daemon"),
                                                 QStringLiteral("Show my clocks"))) {
                    tasks->AddObject(task);
                    task->Release();
                }
                list->AddUserTasks(tasks);
                tasks->Release();
            }
            list->CommitList();
        }
        if (removed)
            removed->Release();
        list->Release();
    }

    if (owned)
        CoUninitialize();
}

}  // namespace jumplist

#else

namespace jumplist {

void install() {}

}  // namespace jumplist

#endif
