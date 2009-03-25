#include "Network.hpp"
#include "Server.hpp"
#include "FairServerMessageQueue.hpp"
#include "Message.hpp"

namespace CBR{
FairServerMessageQueue::FairServerMessageQueue(Network* net, uint32 bytes_per_second, bool renormalizeWeights, const ServerID& sid, Trace* trace)
 : ServerMessageQueue(net, sid, trace),
   mServerQueues(bytes_per_second,0,renormalizeWeights)
{
}

bool FairServerMessageQueue::addMessage(ServerID destinationServer,const Network::Chunk&msg){
    // If its just coming back here, skip routing and just push the payload onto the receive queue
    if (mSourceServer == destinationServer) {
        ChunkSourcePair csp;
        csp.chunk = new Network::Chunk(msg);
        csp.source = mSourceServer;

        mReceiveQueue.push(csp);
        return true;
    }

    uint32 offset = 0;
    Network::Chunk with_header;
    ServerMessageHeader server_header(mSourceServer, destinationServer);
    offset = server_header.serialize(with_header, offset);
    with_header.insert( with_header.end(), msg.begin(), msg.end() );
    offset += msg.size();

    return mServerQueues.queueMessage(destinationServer,new ServerMessagePair(destinationServer,with_header))==QueueEnum::PushSucceeded;
}

bool FairServerMessageQueue::receive(Network::Chunk** chunk_out, ServerID* source_server_out) {
    if (mReceiveQueue.empty()) {
        *chunk_out = NULL;
        return false;
    }

    *chunk_out = mReceiveQueue.front().chunk;
    *source_server_out = mReceiveQueue.front().source;
    mReceiveQueue.pop();

    return true;
}

void FairServerMessageQueue::service(const Time&t){


    std::vector<ServerMessagePair*> finalSendMessages=mServerQueues.tick(t);
    for (std::vector<ServerMessagePair*>::iterator i=finalSendMessages.begin(),ie=finalSendMessages.end();
         i!=ie;
         ++i) {
        mNetwork->send((*i)->dest(),(*i)->data(),false,true,1);
        mTrace->packetSent(t, (*i)->dest(), GetMessageUniqueID((*i)->data()), (*i)->data().size());
    }
    finalSendMessages.resize(0);

    // no limit on receive bandwidth
    while( Network::Chunk* c = mNetwork->receiveOne() ) {
        uint32 offset = 0;
        ServerMessageHeader hdr = ServerMessageHeader::deserialize(*c, offset);
        assert(hdr.destServer() == mSourceServer);
        Network::Chunk* payload = new Network::Chunk;
        payload->insert(payload->begin(), c->begin() + offset, c->end());
        delete c;

        ChunkSourcePair csp;
        csp.chunk = payload;
        csp.source = hdr.sourceServer();

        mReceiveQueue.push(csp);
    }
}

void FairServerMessageQueue::setServerWeight(ServerID sid, float weight) {
    if (!mServerQueues.hasQueue(sid)) {
        mServerQueues.addQueue(new Queue<ServerMessagePair*>(65536),sid,weight);
    }
    else {
        mServerQueues.setQueueWeight(sid, weight);
    }
}
void FairServerMessageQueue::removeServer(ServerID sid) {
    mServerQueues.removeQueue(sid);
}

}
