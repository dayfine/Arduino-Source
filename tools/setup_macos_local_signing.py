import os
import pathlib
import secrets
import subprocess
import tempfile

IDENTITY = 'PokemonAutomation Local Development'
KEYCHAIN = str(pathlib.Path.home() / 'Library/Keychains/login.keychain-db')
ROOT = pathlib.Path.home() / '.local/share/pokemonautomation-signing'
ROOT.mkdir(mode=0o700, parents=True, exist_ok=True)
OPENSSL = '/opt/homebrew/opt/openssl@3/bin/openssl'
os.umask(0o077)

def run(label, args):
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(label + ' failed: ' + result.stderr.strip())
    return result.stdout

existing = subprocess.run(['security', 'find-certificate', '-c', IDENTITY, '-p', KEYCHAIN], capture_output=True, text=True)
certificate = ROOT / 'identity.pem'
if existing.returncode == 0:
    certificate.write_text(existing.stdout)
    print('Reusing existing local certificate.', flush=True)
else:
    with tempfile.TemporaryDirectory(prefix='pa-signing-') as directory:
        temporary = pathlib.Path(directory)
        config = temporary / 'certificate.cnf'
        config.write_text('''[req]
prompt = no
distinguished_name = subject
x509_extensions = usage
[subject]
CN = PokemonAutomation Local Development
[usage]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature
extendedKeyUsage = critical,codeSigning
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
''')
        key = temporary / 'private.pem'
        run('Certificate generation', [OPENSSL, 'req', '-x509', '-newkey', 'rsa:3072', '-nodes', '-sha256', '-days', '3650', '-config', str(config), '-keyout', str(key), '-out', str(certificate)])
        password = secrets.token_urlsafe(32)
        passfile = temporary / 'wrap-password'
        passfile.write_text(password)
        wrapped = temporary / 'identity.p12'
        run('Temporary key wrapping', [OPENSSL, 'pkcs12', '-export', '-legacy', '-inkey', str(key), '-in', str(certificate), '-name', IDENTITY, '-passout', 'file:' + str(passfile), '-out', str(wrapped)])
        run('Login keychain import', ['security', 'import', str(wrapped), '-k', KEYCHAIN, '-P', password, '-x', '-T', '/usr/bin/codesign'])
        print('Imported local identity; temporary key material removed next.', flush=True)
print('Local identity is installed; no certificate trust override is required.', flush=True)
print(run('Certificate verification', ['security', 'find-certificate', '-c', IDENTITY, '-Z', KEYCHAIN]), flush=True)
