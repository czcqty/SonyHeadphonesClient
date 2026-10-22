"""UI-independent private child transport. Call public methods on the UI thread."""
import codecs
import json
import queue
import subprocess
import threading


class ProcessClient:
    START_TIMEOUT = 10
    DIAGNOSTIC_LIMIT = 4096

    def __init__(self, argv, dispatch, changed):
        self.argv, self.dispatch, self.changed = argv, dispatch, changed
        self.state, self.error, self.diagnostic = {}, '', ''
        self.ready, self.closed, self.sent = False, False, 0
        self.process = None
        self._generation = 0
        self._session = None
        self.restart()

    def _post(self, session, callback):
        def run():
            if not self.closed and self._session is session and not session['cancel'].is_set():
                callback()
        self.dispatch(run)

    def restart(self):
        if self.closed:
            return
        old = self._session
        previous_cleanup = self._retire(old) if old else None
        self._generation += 1
        self.state, self.error, self.diagnostic, self.ready = {}, 'backend-starting', '', False
        session = dict(cancel=threading.Event(), outgoing=queue.Queue(64), process=None,
                       diagnostic='', diagnostic_pending=False, timer=None, generation=self._generation, cleanup=None)
        self._session = session
        timer = threading.Timer(self.START_TIMEOUT, lambda: self._post(session, lambda: self._timeout(session)))
        timer.daemon = True
        session['timer'] = timer
        timer.start()
        self.changed()
        threading.Thread(target=self._start, args=(session, previous_cleanup)).start()

    def _start(self, session, previous_cleanup):
        if previous_cleanup:
            previous_cleanup.join()
        if session['cancel'].is_set():
            return
        try:
            process = subprocess.Popen(self.argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, bufsize=0)
            session['process'] = process
            if session['cancel'].is_set():
                self._shutdown(session)
                return
            self._post(session, lambda: setattr(self, 'process', process))
            threading.Thread(target=self._write, args=(session,), daemon=True).start()
            threading.Thread(target=self._diagnostics, args=(session,), daemon=True).start()
            self._read(session)
        except OSError as error:
            self._post(session, lambda detail=str(error): self._fail(session, 'backend-start-failed', detail))

    def _timeout(self, session):
        if not self.ready:
            self._fail(session, 'backend-start-timeout')

    def _diagnostics(self, session):
        decoder = codecs.getincrementaldecoder('utf-8')('replace')
        try:
            while data := session['process'].stderr.read(1024):
                session['diagnostic'] = (session['diagnostic'] + decoder.decode(data))[-self.DIAGNOSTIC_LIMIT:]
                if not session['diagnostic_pending']:
                    session['diagnostic_pending'] = True
                    self._post(session, lambda: self._update_diagnostic(session))
        except (OSError, ValueError):
            pass

    def _update_diagnostic(self, session):
        session['diagnostic_pending'] = False
        self.diagnostic = session['diagnostic']

    def _read(self, session):
        try:
            pending = b''
            while data := session['process'].stdout.read(4096):
                pending += data
                while b'\n' in pending:
                    line, pending = pending.split(b'\n', 1)
                    if len(line) > 1048576:
                        raise ValueError('Oversized backend output')
                    event = json.loads(line)
                    if not isinstance(event, dict):
                        raise ValueError('Expected a backend object')
                    self._post(session, lambda event=event: self._receive(session, event))
                if len(pending) > 1048576:
                    raise ValueError('Oversized backend output')
            self._post(session, lambda: self._fail(session, 'backend-stopped'))
        except (OSError, ValueError, UnicodeError) as error:
            self._post(session, lambda detail=str(error): self._fail(session, 'backend-protocol-error', detail))

    def _receive(self, session, event):
        if event.get('type') == 'state':
            if event.get('version') != 1:
                self._fail(session, 'unsupported-version')
                return
            if not all(isinstance(event.get(key), dict) for key in
                       ('app', 'connection', 'playback', 'noiseControl', 'equalizer', 'settings', 'deviceManager')):
                self._fail(session, 'backend-protocol-error')
                return
            if not self.ready:
                self.error = ''
            self.state, self.ready = event, True
            session['timer'].cancel()
        elif event.get('type') == 'result':
            self.error = '' if event.get('ok') is True else event.get('errorCode', 'Command rejected')
        self.changed()

    def _fail(self, session, code, detail=''):
        self.state, self.ready, self.error = {}, False, code
        self.diagnostic = (detail or session['diagnostic'])[-self.DIAGNOSTIC_LIMIT:]
        self._retire(session)
        self.changed()

    def send(self, request):
        if self.closed or not self.ready:
            return False
        session = self._session
        try:
            frame = json.dumps(dict(request, version=1, id=str(self.sent + 1)), ensure_ascii=False).encode('utf-8') + b'\n'
            session['outgoing'].put_nowait(frame)
            self.sent += 1
            return True
        except queue.Full:
            self._fail(session, 'backend-write-failed', 'Command queue is full')
            return False

    def _write(self, session):
        try:
            while not session['cancel'].is_set():
                try:
                    frame = session['outgoing'].get(timeout=.1)
                except queue.Empty:
                    continue
                if session['cancel'].is_set():
                    break
                stream = session['process'].stdin
                view = memoryview(frame)
                while view:
                    count = stream.write(view)
                    if not count:
                        raise OSError('Backend pipe closed')
                    view = view[count:]
        except (OSError, ValueError) as error:
            self._post(session, lambda detail=str(error): self._fail(session, 'backend-write-failed', detail))

    @staticmethod
    def _shutdown(session):
        process = session['process']
        if process is None:
            return
        try:
            process.stdin.close()
            process.wait(timeout=2)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            try:
                process.kill()
                process.wait(timeout=2)
            except (OSError, subprocess.TimeoutExpired):
                pass
        finally:
            for stream in (process.stdin, process.stdout, process.stderr):
                try:
                    stream.close()
                except (OSError, ValueError):
                    pass

    def _retire(self, session):
        if session['cleanup']:
            return session['cleanup']
        session['cancel'].set()
        if session['timer']:
            session['timer'].cancel()
        # Non-daemon cleanup survives UI shutdown and reaps only our own child.
        thread = threading.Thread(target=self._shutdown, args=(session,))
        session['cleanup'] = thread
        thread.start()
        return thread

    def close(self):
        if self.closed:
            return
        self.closed, self.ready, self.state = True, False, {}
        if self._session:
            self._retire(self._session)
