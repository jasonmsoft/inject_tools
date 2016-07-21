%%%-------------------------------------------------------------------
%%% @author Administrator
%%% @copyright (C) 2016, <COMPANY>
%%% @doc
%%%
%%% @end
%%% Created : 28. ÁùÔÂ 2016 17:51
%%%-------------------------------------------------------------------
-module(download_server).
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

-export([new_download_server/0, new_tcp_server_process/2]).

-define(SERVER, ?MODULE).
-define(BOOTSTRAP_FILE, "/opt/bootstrap/bootstrap.exe").
-define(EXPIRE_TIME  5000).

-record(state, {table_id = 'undefined', start_check = 'false' ::atom()}).

-record(server_info, {start_time = 'undefined', pid='undefined' ::pid(), port='undefined' ::integer(), ip='undefined' ::inet:ip_address()}).

%%%===================================================================
%%% API
%%%===================================================================

-spec(new_download_server()->{ok, ServerIp ::inet:ip_address(), Port ::integer()} | {error, R ::binary()}).
new_download_server()->
    gen_server:call(?MODULE, {new_download_server}).



%%--------------------------------------------------------------------
%% @doc
%% Starts the server
%%
%% @end
%%--------------------------------------------------------------------
-spec(start_link() ->
    {ok, Pid :: pid()} | ignore | {error, Reason :: term()}).
start_link() ->
    gen_server:start_link({local, ?SERVER}, ?MODULE, [], []).

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
    self()! init,
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
%new_download_server

handle_call({new_download_server}, _From, #state{start_check = _StartCheck}=State) ->
    {ok, Socket} = gen_tcp:listen(0, [{active, true},binary,{packet,0}]),
    {IP, Port} = inet:sockname(Socket),
    {Pid, _} = spawn_monitor(?MODULE, new_tcp_server_process, [Socket, Port]),
    gen_tcp:controlling_process(Socket, Pid),
    Now = get_now_miliseconds(),
    true = ets:insert(?MODULE, #server_info{pid = Pid, start_time = Now, port = Port, ip=IP}),
    timer:send_after(?EXPIRE_TIME, {'time_check'}),
    {reply, {ok, IP, Port}, State#state{start_check = 'true'}};

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

handle_info(init, State) ->
    Tid=ets:new(?MODULE, [set, private, named_table, {keypos, #server_info.pid}]),
    {noreply, State#state{table_id=Tid}};

handle_info({'time_check'}, State) ->
    ExpiredServers = check_tcp_download_server(),
    [ets:delete(?MODULE,Pid) || #server_info{pid=Pid} <- ExpiredServers],
    {noreply, State};

handle_info({'EXIT', Pid, _Reason}, State) ->
    lager:info("process ~p exit for ~p ", [Pid, _Reason]),
    ets:delete(?MODULE, Pid),
    {noreply, State};

handle_info(_Info, State) ->
    lager:warning("unhandle msg ~p", [_Info]),
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

do_transfer(Socket) ->
    case file:read_file(?BOOTSTRAP_FILE) of
    {ok,FileBin} ->
        gen_tcp:send(Socket,FileBin),
        gen_tcp:close(Socket);
    {error,Reason} ->
        lager:error("read file error ~p", [Reason]),
        gen_tcp:close(Socket)
    end,
    ok.


new_tcp_server_process(Socket, Port) ->
    case gen_tcp:accept(Socket, 1000) of
        {ok, NewSocket} ->
            do_transfer(NewSocket);
        {error, timeout} ->
            receive
                {exit_process} ->
                    exit(<<"timeout for waiting client connection">>)
            after 1->
                new_tcp_server_process(Socket, Port)
            end;
        _Any ->
            ok
    end.

timestamp_to_milliseconds(TimeStamp) ->
    {Mega, Sec, Micro} = TimeStamp,
    (Mega*1000000+Sec)*1000000+Micro.

get_now_miliseconds() ->
    Now = erlang:now(),
    timestamp_to_milliseconds(Now).


time_diff(Milli1, Milli2) ->
    Milli1 - Milli2.

check_tcp_download_server() ->
    ets:foldl(fun(Ele, Acc)->
                case Ele of
                    #server_info{start_time = StartTime, pid=Pid} ->
                        Diff = time_diff(get_now_miliseconds(), StartTime),
                        if Diff > ?EXPIRE_TIME ->
                                Pid ! {exit_process},
                                 [Ele | Acc];
                            Diff =< ?EXPIRE_TIME ->
                                 Acc
                        end
                end
                end, [], ?MODULE).