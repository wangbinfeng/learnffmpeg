//
//  RtspClient.cpp
//  testboost
//
//  Created by 王斌峰 on 2018/9/30.
//  Copyright © 2018年 Binfeng. All rights reserved.
//
#include "RtspClient.hpp"

#include <iostream>
#include <memory>

#include "boost/algorithm/string.hpp"

#include "RtspRequest.hpp"


using namespace std;

[[maybe_unused]]
static void waitForTerminated() {
    sigset_t sset;
    sigemptyset(&sset);
    sigaddset(&sset, SIGINT);
    sigaddset(&sset, SIGQUIT);
    sigaddset(&sset, SIGTERM);
    sigaddset(&sset, SIGHUP);
    sigprocmask(SIG_BLOCK, &sset, NULL);
    int sig;
    sigwait(&sset, &sig);
}

void RtspClient::start() {
    BoostCommandExecutor *exec = new BoostCommandExecutor(ip, port);
    RtspResponse res;

    unique_ptr<RtspOptionsRequest> options = make_unique<RtspOptionsRequest>(url);
    options->setCseq("1");
    cout << options->to_string() << endl;
    unique_ptr<RtspCommand> optioncommand = make_unique<RtspCommand>(options.get(), &res, exec);
    optioncommand->run();
    cout << res.respDate << endl;
    cout << res.cseq << endl;
    cout << res.respPublic << endl;

    RtspResponse* res1 = new RtspResponse;
    RtspDescribeRequest describe{url};
    describe.setCseq("2");
    cout << "\n" << describe.to_string() << endl;
    unique_ptr<RtspCommand> describeCmd = make_unique<RtspCommand>(&describe, res1, exec);
    describeCmd->run();

    RtspResponse* res2 = new RtspResponse;
    RtspSetupRequest setup{url};
    setup.setCseq("3");
    cout <<"\n\n" <<setup.to_string() << endl;
    unique_ptr<RtspCommand> setupCmd = make_unique<RtspCommand>(&setup, res2, exec);
    setupCmd->run();

    session = new RtspSession(res2->session);
    cout<< "session:" << res2->transport << endl;
    session->setTransport(res2->transport);
    session->setBody(res1->body);
    delete res2;
    delete res1;

    session->start();

    RtspResponse* res3 = new RtspResponse;
    RtspPlayRequest play{url};
    play.setCseq("4");
    play.setSession(session->getSessionId());
    cout << play.to_string() << endl;
    unique_ptr<RtspCommand> playCmd = make_unique<RtspCommand>(&play, res3, exec);
    playCmd->run();
    session->setRTPInfo(res3->rtpInfo.c_str());

    delete res3;
}

void RtspClient::close() {
    if ( exec != nullptr ) {
        cout << "delet exec.." << endl;
        delete exec;
        exec = nullptr;
    }

    cout << "close session" << endl;
    if ( session != nullptr ) {
        session->close();
        delete session;
        session = nullptr;
    }
}
