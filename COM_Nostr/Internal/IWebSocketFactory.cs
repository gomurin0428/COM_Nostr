using System.Runtime.InteropServices;

namespace COM_Nostr.Internal;

[ComVisible(false)]
public interface IWebSocketFactory
{
    object Create();
}
