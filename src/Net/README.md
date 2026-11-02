# Net

Verbindung zwischen den Spielern: Transport-Tunnel, Steam-Lobby, Session, Handshake und Serialisierung.
Bindet kein SDK ein und ist deshalb ohne Spiel testbar. Greift nie direkt auf Unreal-Objekte zu.

Einstieg ist `Net::Start` in `Net/Net.h`, noch ohne Inhalt. Darf `Core` nutzen. Daten für den Game Thread gehen über `Core::GameThread().Post`.
