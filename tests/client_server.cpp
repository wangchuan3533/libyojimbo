/*
    Client/Server Testbed

    Copyright © 2016, The Network Protocol Company, Inc.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

        2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
           in the documentation and/or other materials provided with the distribution.

        3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived 
           from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define SERVER 1
#define CLIENT 1
#define MATCHER 1
#define LOGGING 1

#include "shared.h"

int ClientServerMain()
{
    LocalMatcher matcher;

    uint64_t clientId = 1;

    uint8_t connectTokenData[ConnectTokenBytes];
    uint8_t connectTokenNonce[NonceBytes];

    uint8_t clientToServerKey[KeyBytes];
    uint8_t serverToClientKey[KeyBytes];

    int numServerAddresses;
    Address serverAddresses[MaxServersPerConnect];

    memset( connectTokenNonce, 0, NonceBytes );

    uint64_t connectTokenExpireTimestamp;

    if ( !matcher.RequestMatch( clientId, connectTokenData, connectTokenNonce, clientToServerKey, serverToClientKey, connectTokenExpireTimestamp, numServerAddresses, serverAddresses ) )
    {
        printf( "error: request match failed\n" );
        return 1;
    }

    Address clientAddress( "::1", ClientPort );
    Address serverAddress( "::1", ServerPort );

    double time = 100.0;

    NetworkTransport clientTransport( GetDefaultAllocator(), clientAddress, ProtocolId, time );
    NetworkTransport serverTransport( GetDefaultAllocator(), serverAddress, ProtocolId, time );

    if ( clientTransport.GetError() != SOCKET_ERROR_NONE || serverTransport.GetError() != SOCKET_ERROR_NONE )
    {
        printf( "error: failed to initialize sockets\n" );
        return 1;
    }

    clientTransport.SetNetworkConditions( 250, 250, 5, 10 );
    serverTransport.SetNetworkConditions( 250, 250, 5, 10 );

    GameClient client( GetDefaultAllocator(), clientTransport, ClientServerConfig(), time );

    GameServer server( GetDefaultAllocator(), serverTransport, ClientServerConfig(), time );

    server.SetServerAddress( serverAddress );

    server.Start();
    
    client.Connect( clientId, serverAddress, connectTokenData, connectTokenNonce, clientToServerKey, serverToClientKey, connectTokenExpireTimestamp );

    int result = 1;

    const int NumIterations = 256;

    for ( int iteration = 0; iteration < NumIterations; ++iteration )
    {
        client.SendPackets();
        server.SendPackets();

        clientTransport.WritePackets();
        serverTransport.WritePackets();

        clientTransport.ReadPackets();
        serverTransport.ReadPackets();

        client.ReceivePackets();
        server.ReceivePackets();

        client.CheckForTimeOut();
        server.CheckForTimeOut();

        if ( client.ConnectionFailed() )
        {
            printf( "error: client connect failed!\n" );
            break;
        }

        if ( client.IsConnected() && server.GetNumConnectedClients() == 1 )
        {
            // success
            client.Disconnect();
            result = 0;
        }

        time += 0.1f;

        if ( !client.IsConnecting() && !client.IsConnected() && server.GetNumConnectedClients() == 0 )
            break;

        client.AdvanceTime( time );
        server.AdvanceTime( time );

        clientTransport.AdvanceTime( time );
        serverTransport.AdvanceTime( time );
    }

    client.Disconnect();

    server.Stop();

    return result;
}

int main()
{
    printf( "\n" );

    verbose_logging = true;

    if ( !InitializeYojimbo() )
    {
        printf( "error: failed to initialize Yojimbo!\n" );
        return 1;
    }

    srand( (unsigned int) time( NULL ) );

    int result = ClientServerMain();

    ShutdownYojimbo();

    printf( "\n" );

    return result;
}
