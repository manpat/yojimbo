/*
    Yojimbo Client/Server Network Protocol Library.
    
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

#ifndef YOJIMBO_MESSAGE_H
#define YOJIMBO_MESSAGE_H

#include "yojimbo_config.h"
#include "yojimbo_stream.h"
#include "yojimbo_serialize.h"
#include "yojimbo_allocator.h"
#include "yojimbo_bit_array.h"

#if YOJIMBO_DEBUG_MESSAGE_LEAKS
#include <map>
#endif // #if YOJIMBO_DEBUG_MESSAGE_LEAKS

namespace yojimbo
{
    /**
        A message is a reference counted object that knows how to serialize itself to and from a bitstream.

        The typical usage is to create your own set of message classes by inheriting from this class, or from BlockMessage, if you want to attach data blocks to your message.

        Then you setup an enum of all your message types, and derive a message factory class to create message instances by type. The set of messages must be known at compile time. 

        There are macros to help make declaring your message factory painless:

            YOJIMBO_MESSAGE_FACTORY_START
            YOJIMBO_DECLARE_MESSAGE_TYPE
            YOJIMBO_MESSAGE_FACTORY_FINISH

        Once you have a message factory, register it with your declared inside your client and server classes using:

            YOJIMBO_MESSAGE_FACTORY

        which overrides Client::CreateMessageFactory and Server::CreateMessageFactory methods to create your message factory type. 

        It's done this way for security reasons, so each client has their own message factory and allocator on the server.  This is done so each client is silo'd to their own set of resources.

        Look at tests/shared.h for an example showing how to set up messages and a message factory, and how to configure the message factory on the client and server with this macro:
        
            YOJIMBO_MESSAGE_FACTORY

        @see BlockMessage
        @see MessageFactory
        @see ClientServerConfig
        @see Connection
        @see ConnectionConfig
        @see ChannelConfig
     */

    class Message : public Serializable
    {
    public:

        /**
            Message constructor.

            Don't call this directly, use a message factory instead.

            @param blockMessage 1 if this is a block message, 0 otherwise.

            @see MessageFactory::Create
         */

        Message( int blockMessage = 0 ) : m_refCount(1), m_id(0), m_type(0), m_blockMessage( blockMessage ) {}

        /** 
            Set the message id.

            When messages are sent across the ReliableOrderedChannel, the message id starts at 0 and increases with each message sent over that channel, wrapping around from 65535 -> 0. It's used to reconstruct message order on the receiver, so messages are received in the same order they were sent.

            Over the UnreliableUnorderedChannel, there is no ordering, so the message id is set to the sequence number of the connection packet instead.

            @param id The message id.
         */

        void SetId( uint16_t id ) { m_id = id; }

        /**
            Get the message id.

            @returns The message id.
         */

        int GetId() const { return m_id; }

        /**
            Get the message type.

            This corresponds to the type enum value used to create the message in the message factory.

            @returns The message type.

            @see MessageFactory.
         */

        int GetType() const { return m_type; }

        /**
            Get the reference count on the message.

            Messages start with a reference count of 1 when they are created. This is decreased when they are released. 

            When the reference count reaches 0, the message is destroyed.

            @returns The reference count on the message.
         */

        int GetRefCount() { return m_refCount; }

        /**
            Is this a block message?

            Block messages are of type BlockMessage and can have a data block attached to the message.

            @returns True if this is a block message, false otherwise.

            @see BlockMessage.
         */

        bool IsBlockMessage() const { return m_blockMessage; }

        /**
            Virtual serialize function (read).

            Reads the message in from a bitstream.

            Don't override this method directly, instead, use the YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS macro in your derived message class redirect it to a templated serialize method.

            This way you can implement read and write for your messages in a single method and the C++ compiler takes care of generating specialized read and write. See tests/shared.h for examples.
         */

        virtual bool SerializeInternal( ReadStream & stream ) = 0;

        /**
            Virtual serialize function (write).

            Write the message to a bitstream.

            Don't override this method directly, instead, use the YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS macro in your derived message class redirect it to a templated serialize method.

            This way you can implement read and write for your messages in a single method and the C++ compiler takes care of generating specialized read and write. See tests/shared.h for examples.
         */

        virtual bool SerializeInternal( WriteStream & stream ) = 0;

        /**
            Virtual serialize function (measure).

            Measure how many bits this message would take to write. This is used when working out how many messages will fit within the channel packet budget.

            Don't override this method directly, instead, use the YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS macro in your derived message class redirect it to a templated serialize method.
         */

        virtual bool SerializeInternal ( MeasureStream & stream ) = 0;

    protected:

        /**
            Set the message type.

            Called by the message factory after it creates a message.

            @param type The message type.
         */

        void SetType( int type ) { m_type = type; }

        /**
            Add a reference to the message.

            This is called when a message is included in a packet and added to the receive queue. 

            This way we don't have to pass messages by value (more efficient) and messages get cleaned up when they are delivered and no packets refer to them.
         */

        void AddRef() { m_refCount++; }

        /**
            Remove a reference from the message.

            Message are deleted when the number of references reach zero. Messages have reference count of 1 after creation.
         */

        void Release() { assert( m_refCount > 0 ); m_refCount--; }

        /**
            Message destructor.

            Protected because you aren't supposed delete messages directly because they are reference counted. Use MessageFactory::Release instead.

            @see MessageFactory::Release
         */

        virtual ~Message()
        {
            assert( m_refCount == 0 );
        }

    private:

        friend class MessageFactory;
      
        Message( const Message & other );
        
        const Message & operator = ( const Message & other );

        int m_refCount;                                                     ///< Number of references on this message object. Starts at 1. Message is destroyed when it reaches 0.
        uint32_t m_id : 16;                                                 ///< The message id. For messages sent over reliable-ordered channels, this starts at 0 and increases with each message sent. For unreliable-unordered channels this is set to the sequence number of the packet the message was included in.
        uint32_t m_type : 15;                                               ///< The message type. Corresponds to the type integer used when the message was created though the message factory.
        uint32_t m_blockMessage : 1;                                        ///< 1 if this is a block message. 0 otherwise. If 1 then you can cast the Message* to BlockMessage*. In short, it's a lightweight RTTI.
    };

    /**
        A message that can have a block of data attached to it.

        Attaching blocks of data is very useful, especially over a reliable-ordered channel where these blocks can be larger that the maximum packet size. Blocks sent over a reliable-ordered channel are automatically split up into fragments and reassembled on the other side.

        This gives you have the convenience of a reliable-ordered control messages, while attaching large blocks of data (larger than max packet size), while having all messages delivered reliably and in-order. 

        Situations where this can be useful is when sending down the initial state of the world on client connect, or block of configuration data to send up from the client to server on connect.

        It can also be used for messages sent across an unreliable-unordered channel, but in that case blocks aren't split up into fragments. Make sure you consider this when designing your channel budgets when sending blocks over unreliable-unordered channels.

        @see ChannelConfig
     */

    class BlockMessage : public Message
    {
    public:

        /**
            Block message constructor.

            Don't call this directly, use a message factory instead.

            @see MessageFactory::Create
         */

        explicit BlockMessage() : Message( 1 ), m_allocator(NULL), m_blockData(NULL), m_blockSize(0) {}

        /**
            Attach a block to this message.

            You can only attach one block. This method will assert if a block is already attached.
         */

        void AttachBlock( Allocator & allocator, uint8_t * blockData, int blockSize )
        {
            assert( blockData );
            assert( blockSize > 0 );
            assert( !m_blockData );

            m_allocator = &allocator;
            m_blockData = blockData;
            m_blockSize = blockSize;
        }

        /** 
            Detach the block from this message.

            By doing this you are responsible for copying the block pointer and allocator and making sure the block is freed.

            This could be used for example, if you wanted to copy off the block and store it somewhere, without the cost of copying it.
         */

        void DetachBlock()
        {
            m_allocator = NULL;
            m_blockData = NULL;
            m_blockSize = 0;
        }

        /**
            Get the allocator used to allocate the block.

            @returns The allocator for the block. NULL if no block is attached to this message.
         */

        Allocator * GetAllocator()
        {
            return m_allocator;
        }

        /**
            Get the block data pointer.

            @returns The block data pointer. NULL if no block is attached.
         */

        uint8_t * GetBlockData()
        {
            return m_blockData;
        }

        /**
            Get the size of the block attached to this message.

            @returns The size of the block (bytes). 0 if no block is attached.
         */

        int GetBlockSize() const
        {
            return m_blockSize;
        }

        /**
            Templated serialize function for the block message. Doesn't do anything. The block data is serialized elsewhere.

            You can override the serialize methods on a block message to implement your own serialize function. It's just like a regular message with a block attached to it.

            @see ConnectionPacket
            @see ChannelPacketData
            @see ReliableOrderedChannel
            @see UnreliableUnorderedChannel
         */

        template <typename Stream> bool Serialize( Stream & stream ) { (void) stream; return true; }

        YOJIMBO_VIRTUAL_SERIALIZE_FUNCTIONS();

    protected:

        /**
            If a block was attached to the message, it is freed here.
         */

        ~BlockMessage()
        {
            if ( m_allocator )
            {
                YOJIMBO_FREE( *m_allocator, m_blockData );
                m_blockSize = 0;
                m_allocator = NULL;
            }
        }

    private:

        Allocator * m_allocator;                                                ///< Allocator for the block attached to the message. NULL if no block is attached.
        uint8_t * m_blockData;                                                  ///< The block data. NULL if no block is attached.
        int m_blockSize;                                                        ///< The block size (bytes). 0 if no block is attached.
    };

    /**
        Message factory error level.
     */

    enum MessageFactoryError
    {
        MESSAGE_FACTORY_ERROR_NONE,                                             ///< No error. All is well.
        MESSAGE_FACTORY_ERROR_FAILED_TO_ALLOCATE_MESSAGE,                       ///< Failed to allocate a message. Typically this means we ran out of memory on the allocator backing the message factory.
    };

    /**
        Defines the set of message types that can be created.

        You can derive a message factory yourself to create your own message types, or you can use these helper macros to do it for you:

            YOJIMBO_MESSAGE_FACTORY_START
            YOJIMBO_DECLARE_MESSAGE_TYPE
            YOJIMBO_MESSAGE_FACTORY_FINISH

        See tests/shared.h for an example showing how to use the macros.
     */

    class MessageFactory
    {        
        #if YOJIMBO_DEBUG_MESSAGE_LEAKS
        std::map<void*,int> allocated_messages;                                 ///< The set of allocated messages for this factory. Used to track dowm message leaks.
        #endif // #if YOJIMBO_DEBUG_MESSAGE_LEAKS

        Allocator * m_allocator;                                                ///< The allocator used to create messages.

        int m_numTypes;                                                         ///< The number of message types.
        
        int m_error;                                                            ///< The message factory error level.

    public:

        /**
            Message factory allocator.

            Pass in the number of message types for the message factory from the derived class.

            @param allocator The allocator used to create messages.
            @param numTypes The number of message types. Valid types are in [0,numTypes-1].
         */

        MessageFactory( Allocator & allocator, int numTypes )
        {
            m_allocator = &allocator;
            m_numTypes = numTypes;
            m_error = MESSAGE_FACTORY_ERROR_NONE;
        }

        /**
            Message factory destructor.

            Checks for message leaks if YOJIMBO_DEBUG_MESSAGE_LEAKS is defined and not equal to zero. This is on by default in debug build.
         */

        virtual ~MessageFactory()
        {
            assert( m_allocator );

            m_allocator = NULL;

            #if YOJIMBO_DEBUG_MESSAGE_LEAKS
            if ( allocated_messages.size() )
            {
                printf( "you leaked messages!\n" );
                printf( "%d messages leaked\n", (int) allocated_messages.size() );
                typedef std::map<void*,int>::iterator itor_type;
                for ( itor_type i = allocated_messages.begin(); i != allocated_messages.end(); ++i ) 
                {
                    Message * message = (Message*) i->first;
                    printf( "leaked message %p (type %d, refcount %d)\n", message, message->GetType(), message->GetRefCount() );
                }
                exit(1);
            }
            #endif // #if YOJIMBO_DEBUG_MESSAGE_LEAKS
        }

        /**
            Create a message by type.

            Messages returned from this function have one reference added to them. When you are finished with the message, pass the message to MessageFactory::Release.

            @param type The message type in [0,numTypes-1].

            @returns The allocated message, or NULL if the message could not be allocated. If the message allocation fails, the message factory error level is set to MESSAGE_FACTORY_ERROR_FAILED_TO_ALLOCATE_MESSAGE.

            @see MessageFactory::AddRef
            @see MessageFactory::Release
         */

        Message * Create( int type )
        {
            assert( type >= 0 );
            assert( type < m_numTypes );

            Message * message = CreateInternal( type );

            if ( !message )
            {
                m_error = MESSAGE_FACTORY_ERROR_FAILED_TO_ALLOCATE_MESSAGE;
                return NULL;
            }

            #if YOJIMBO_DEBUG_MESSAGE_LEAKS
            allocated_messages[message] = 1;
            assert( allocated_messages.find( message ) != allocated_messages.end() );
            #endif // #if YOJIMBO_DEBUG_MESSAGE_LEAKS

            return message;
        }

        /**
            Add a reference to a message.

            @param message The message to add a reference to.

            @see MessageFactory::Create
            @see MessageFactory::Release
         */   

        void AddRef( Message * message )
        {
            assert( message );
            if ( !message )
                return;

            message->AddRef();
        }

        /**
            Remove a reference from a message.

            Messages have 1 reference when created. When the reference count reaches 0, they are destroyed.

            @see MessageFactory::Create
            @see MessageFactory::AddRef
         */

        void Release( Message * message )
        {
            assert( message );
            if ( !message )
                return;

            message->Release();
            
            if ( message->GetRefCount() == 0 )
            {
                #if YOJIMBO_DEBUG_MESSAGE_LEAKS
                assert( allocated_messages.find( message ) != allocated_messages.end() );
                allocated_messages.erase( message );
                #endif // #if YOJIMBO_DEBUG_MESSAGE_LEAKS
            
                assert( m_allocator );

                YOJIMBO_DELETE( *m_allocator, Message, message );
            }
        }

        /**
            Get the number of message types supported by this message factory.

            @returns The number of message types.
         */

        int GetNumTypes() const
        {
            return m_numTypes;
        }

        /**
            Get the allocator used to create messages.

            @returns The allocator.
         */

        Allocator & GetAllocator()
        {
            assert( m_allocator );
            return *m_allocator;
        }

        /**
            Get the error level.

            When used with a client or server, an error level on a message factory other than MESSAGE_FACTORY_ERROR_NONE triggers a client disconnect.
         */

        int GetError() const
        {
            return m_error;
        }

        /**
            Clear the error level back to no error.
         */

        void ClearError()
        {
            m_error = MESSAGE_FACTORY_ERROR_NONE;
        }

    protected:

        /**
            This method is overridden to create messages by type.

            @param type The type of message to be created.

            @returns The message created. Its reference count is 1.
         */

        virtual Message * CreateInternal( int type ) { (void) type; return NULL; }

        /**
            Set the message type of a message.

            Put here because Message::SetMessageType is protected, but we need to be able to call this inside the overridden MessageFactory::CreateInternal method.
            
            @param message The message object.
            @param type The message type to set.
         */

        void SetMessageType( Message * message, int type ) { message->SetType( type ); }
    };
}

// Helper macros to make it easier to setup message factories for your message types. See tests/shared.h for an example of usage!

#define YOJIMBO_MESSAGE_FACTORY_START( factory_class, base_factory_class, num_message_types )                                           \
                                                                                                                                        \
    class factory_class : public base_factory_class                                                                                     \
    {                                                                                                                                   \
    public:                                                                                                                             \
        factory_class( yojimbo::Allocator & allocator = yojimbo::GetDefaultAllocator(), int numMessageTypes = num_message_types )       \
         : base_factory_class( allocator, numMessageTypes ) {}                                                                          \
        yojimbo::Message * CreateInternal( int type )                                                                                   \
        {                                                                                                                               \
            yojimbo::Message * message = base_factory_class::CreateInternal( type );                                                    \
            if ( message )                                                                                                              \
                return message;                                                                                                         \
            yojimbo::Allocator & allocator = GetAllocator();                                                                            \
            (void)allocator;                                                                                                            \
            switch ( type )                                                                                                             \
            {                                                                                                                           \


#define YOJIMBO_DECLARE_MESSAGE_TYPE( message_type, message_class )                                                                     \
                                                                                                                                        \
                case message_type:                                                                                                      \
                    message = YOJIMBO_NEW( allocator, message_class );                                                                  \
                    if ( !message )                                                                                                     \
                        return NULL;                                                                                                    \
                    SetMessageType( message, message_type );                                                                            \
                    return message;

#define YOJIMBO_MESSAGE_FACTORY_FINISH()                                                                                                \
                                                                                                                                        \
                default: return NULL;                                                                                                   \
            }                                                                                                                           \
        }                                                                                                                               \
    };

#endif
