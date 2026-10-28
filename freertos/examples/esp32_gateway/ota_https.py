"""Bounded HTTPS file download for ESP32; no redirects or chunked bodies."""
import socket
import ssl


def split_url(url):
    if not url.startswith("https://") or "\r" in url or "\n" in url or "#" in url:
        raise ValueError("HTTPS URL required")
    rest = url[8:]
    host, sep, path = rest.partition("/")
    if not host or any(c in host for c in "@:? "):
        raise ValueError("invalid HTTPS host")
    return host, "/" + path if sep else "/"


def line(sock, budget):
    data = bytearray()
    while len(data) < budget:
        byte = sock.read(1)
        if not byte:
            raise OSError("truncated HTTP headers")
        data.extend(byte)
        if data.endswith(b"\r\n"):
            return bytes(data)
    raise ValueError("HTTP header too long")


def download(url, target, limit, ca_file):
    host, path = split_url(url)
    if limit <= 0:
        raise ValueError("invalid download limit")
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.verify_mode = ssl.CERT_REQUIRED
    context.load_verify_locations(cafile=ca_file)
    address = socket.getaddrinfo(host, 443, 0, socket.SOCK_STREAM)[0][-1]
    raw = socket.socket()
    stream = None
    try:
        raw.settimeout(20)
        raw.connect(address)
        stream = context.wrap_socket(raw, server_hostname=host)
        request = ("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n" % (path, host)).encode()
        written = 0
        while written < len(request):
            n = stream.write(request[written:])
            if not n:
                raise OSError("short HTTP request write")
            written += n
        status = line(stream, 1024).split()
        if len(status) < 2 or status[0] not in (b"HTTP/1.1", b"HTTP/1.0") or status[1] != b"200":
            raise ValueError("HTTP response is not 200")
        remaining_headers = 8192
        length = None
        while True:
            header = line(stream, min(remaining_headers, 1024))
            remaining_headers -= len(header)
            if header == b"\r\n":
                break
            if b":" not in header:
                raise ValueError("invalid HTTP header")
            key, value = header[:-2].split(b":", 1)
            key, value = key.strip().lower(), value.strip()
            if key == b"transfer-encoding":
                raise ValueError("transfer encoding unsupported")
            if key == b"content-length":
                if length is not None or not value.isdigit():
                    raise ValueError("invalid content length")
                length = int(value)
        if length is None or length <= 0 or length > limit:
            raise ValueError("invalid download size")
        remaining = length
        while remaining:
            chunk = stream.read(min(1024, remaining))
            if not chunk:
                raise OSError("truncated download")
            if target.write(chunk) != len(chunk):
                raise OSError("short file write")
            remaining -= len(chunk)
        return length
    finally:
        if stream is not None:
            stream.close()
        else:
            raw.close()
