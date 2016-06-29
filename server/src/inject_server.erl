%%%-------------------------------------------------------------------
%%% @author Administrator
%%% @copyright (C) 2016, <COMPANY>
%%% @doc
%%%
%%% @end
%%% Created : 28. ÁùÔÂ 2016 13:47
%%%-------------------------------------------------------------------
-module(inject_server).
-author("Administrator").

-behaviour(gen_server).

%% API
-export([start_link/0]).

%% gen_server callbacks
-export([init/1,
    handle_call/3,
    handle_cast/2,
    handle_info/2,
    terminate/2,
    code_change/3]).

-export([start/0, stop/0, process_client_request/2]).

-define(SERVER, ?MODULE).

-record(state, {
    socket = 'undefined'
}).

-include("../deps/include/lager.hrl").
-include_lib("../include/client_mgr.hrl").
-compile([{parse_transform, lager_transform}]).

-define(SERVER_PORT, 6489).

-define(MSG_TYPE_GET_BOOTSTRAP_VERSION, 2).
-define(MSG_TYPE_REPLY_BOOTSTRAP_VERSION, 3).
-define(MSG_TYPE_REGISTER, 4).
-define(MSG_TYPE_DOWNLOAD_BOOTSTRAP_REQ, 5).
-define(MSG_TYPE_DOWNLOAD_BOOTSTRAP_REPLY, 6).
-define(VERSION, 1.0).
-define(HDR_LEN, 14).

-define(SERVER_STATE_SEND_VER_REPLY, 'send_ver_reply').
-define(SERVER_STATE_SEND_DOWNLOAD_REPLY, 'send_download_reply').

%%%===================================================================
%%% API
%%%===================================================================

%%--------------------------------------------------------------------
%% @doc
%% Starts the server
%%
%% @end
%%--------------------------------------------------------------------
-spec(start_link() ->
    {ok, Pid :: pid()} | ignore | {error, Reason :: term()}).
start_link() ->
    _ = lager:start(),
    gen_server:start_link({local, ?SERVER}, ?MODULE, [], []).





-spec(start() -> ok).
start()->
    application:start(?MODULE).


-spec(stop() -> ok).
stop() ->
    application:stop(?MODULE).







%%%===================================================================
%%% gen_server callbacks
%%%===================================================================

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Initializes the server
%%
%% @spec init(Args) -> {ok, State} |
%%                     {ok, State, Timeout} |
%%                     ignore |
%%                     {stop, Reason}
%% @end
%%--------------------------------------------------------------------
-spec(init(Args :: term()) ->
    {ok, State :: #state{}} | {ok, State :: #state{}, timeout() | hibernate} |
    {stop, Reason :: term()} | ignore).
init([]) ->
    self() ! 'init',
    {ok, #state{}}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handling call messages
%%
%% @end
%%--------------------------------------------------------------------
-spec(handle_call(Request :: term(), From :: {pid(), Tag :: term()},
    State :: #state{}) ->
    {reply, Reply :: term(), NewState :: #state{}} |
    {reply, Reply :: term(), NewState :: #state{}, timeout() | hibernate} |
    {noreply, NewState :: #state{}} |
    {noreply, NewState :: #state{}, timeout() | hibernate} |
    {stop, Reason :: term(), Reply :: term(), NewState :: #state{}} |
    {stop, Reason :: term(), NewState :: #state{}}).
handle_call(_Request, _From, State) ->
    {reply, ok, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handling cast messages
%%
%% @end
%%--------------------------------------------------------------------
-spec(handle_cast(Request :: term(), State :: #state{}) ->
    {noreply, NewState :: #state{}} |
    {noreply, NewState :: #state{}, timeout() | hibernate} |
    {stop, Reason :: term(), NewState :: #state{}}).
handle_cast(_Request, State) ->
    {noreply, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Handling all non call/cast messages
%%
%% @spec handle_info(Info, State) -> {noreply, State} |
%%                                   {noreply, State, Timeout} |
%%                                   {stop, Reason, State}
%% @end
%%--------------------------------------------------------------------
-spec(handle_info(Info :: timeout() | term(), State :: #state{}) ->
    {noreply, NewState :: #state{}} |
    {noreply, NewState :: #state{}, timeout() | hibernate} |
    {stop, Reason :: term(), NewState :: #state{}}).

handle_info('init', State) ->
    {ok, Socket} = gen_udp:open(?SERVER_PORT,[{binary, {active, true}}]),
    ok = gen_udp:controlling_process(Socket, self()),

    {noreply, State#state{socket = Socket}};



%%{udp, Socket, IP, InPortNo, Packet}
handle_info({udp, _Socket, IP, InPortNo, Packet}, State) ->
    case client_mgr:find_client(IP, InPortNo) of
        {ok, Client} ->
            spawn_monitor(?MODULE, process_client_request, [Client, Packet]);
        {error, _} ->
            NewClient = #client_info{client_addr = {IP, InPortNo}, client_sock = _Socket},
            client_mgr:add_client(NewClient),
            spawn_monitor(?MODULE, process_client_request, [NewClient, Packet])
    end,
    {noreply, State};

handle_info(_Info, State) ->
    lager:info("unhandle info msg ~p ", [_Info]),
    {noreply, State}.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% This function is called by a gen_server when it is about to
%% terminate. It should be the opposite of Module:init/1 and do any
%% necessary cleaning up. When it returns, the gen_server terminates
%% with Reason. The return value is ignored.
%%
%% @spec terminate(Reason, State) -> void()
%% @end
%%--------------------------------------------------------------------
-spec(terminate(Reason :: (normal | shutdown | {shutdown, term()} | term()),
    State :: #state{}) -> term()).
terminate(_Reason, _State) ->
    ok.

%%--------------------------------------------------------------------
%% @private
%% @doc
%% Convert process state when code is changed
%%
%% @spec code_change(OldVsn, State, Extra) -> {ok, NewState}
%% @end
%%--------------------------------------------------------------------
-spec(code_change(OldVsn :: term() | {down, term()}, State :: #state{},
    Extra :: term()) ->
    {ok, NewState :: #state{}} | {error, Reason :: term()}).
code_change(_OldVsn, State, _Extra) ->
    {ok, State}.

%%%===================================================================
%%% Internal functions
%%%===================================================================

create_version_reply() ->
    <<?HDR_LEN:16/unsigned-little-integer,
    ?VERSION:64/unsigned-little-float,
    ?MSG_TYPE_REPLY_BOOTSTRAP_VERSION:16/unsigned-little-integer,
    0:16/unsigned-little-integer>>.

create_download_reply(ServerIP, ServerPort)->
    <<?HDR_LEN:16/unsigned-little-integer,
    ?VERSION:64/unsigned-little-float,
    ?MSG_TYPE_REPLY_BOOTSTRAP_VERSION:16/unsigned-little-integer,
    0:16/unsigned-little-integer, ServerIP:32/unsigned-big-integer, ServerPort:16/unsigned-big-integer>>.



on_get_version(#client_info{client_addr = {IP, Port}, client_sock = Socket, client_state = _State}=_Client, _Body) ->
    Reply = create_version_reply(),
    case gen_udp:send(Socket, IP, Port, Reply) of
        ok ->
            client_mgr:update_client_state(IP, Port, ?SERVER_STATE_SEND_VER_REPLY);
        _->
            lager:error("send version reply to client(~p:~p) error ", [IP, Port])

    end.

on_register(#client_info{client_addr = {IP, Port}}=_Client, Body) ->
    <<OSVer:16/unsigned-little-integer,
    LocalIP:32/unsigned-little-integer, NumPartition:8/unsigned-little-integer,
    MacAddr:48/unsigned-little-integer>> = Body,
    Info={OSVer, LocalIP, NumPartition, MacAddr},
    client_mgr:update_client(IP, Port, Info).

on_download_reguest(Client, Body) ->
    Reply = create_download_reply().

on_default(Client, Body) ->
    .

process_client_request(Client, Packet) ->
    <<_TotalLen:16, ?VERSION:64, MsgType:16, _BodyLen:16, _Body/binary>> = Packet,
    case MsgType of
        ?MSG_TYPE_GET_BOOTSTRAP_VERSION ->
            on_get_version(Client, _Body);
        ?MSG_TYPE_REGISTER ->
            on_register(Client, _Body);
        ?MSG_TYPE_DOWNLOAD_BOOTSTRAP_REQ ->
            on_download_reguest(Client, _Body);
        _Any ->
            on_default(Client, _Body)
    end

