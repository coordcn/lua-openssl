print('ENTER to continue')
io.read()

local openssl = require('openssl')
asn1 = openssl.asn1

certasstring = [[-----BEGIN CERTIFICATE-----
MIIDATCCArCgAwIBAgITEgAFDVkfna1KLEIuKgAAAAUNWTAIBgYqhQMCAgMwfzEj
MCEGCSqGSIb3DQEJARYUc3VwcG9ydEBjcnlwdG9wcm8ucnUxCzAJBgNVBAYTAlJV
MQ8wDQYDVQQHEwZNb3Njb3cxFzAVBgNVBAoTDkNSWVBUTy1QUk8gTExDMSEwHwYD
VQQDExhDUllQVE8tUFJPIFRlc3QgQ2VudGVyIDIwHhcNMTUwNjEzMTczNjQ4WhcN
MTUwOTEzMTc0NjQ4WjATMREwDwYDVQQDEwhuZ2F0ZS5ydTBjMBwGBiqFAwICEzAS
BgcqhQMCAiQABgcqhQMCAh4BA0MABEBn4s6r6zCgimGfiHg4o0FpNaGv1jGzmqSD
chsnAiqcV8fQ4Y6p/o0x8CZEXAC+hzdf5w2f1VxzbJaGCTQslmNYo4IBbTCCAWkw
EwYDVR0lBAwwCgYIKwYBBQUHAwEwCwYDVR0PBAQDAgQwMB0GA1UdDgQWBBT4x4Lz
iE6QcS3Qnmz03HNroSojbzAfBgNVHSMEGDAWgBQVMXywjRreZtcVnElSlxckuQF6
gzBZBgNVHR8EUjBQME6gTKBKhkhodHRwOi8vdGVzdGNhLmNyeXB0b3Byby5ydS9D
ZXJ0RW5yb2xsL0NSWVBUTy1QUk8lMjBUZXN0JTIwQ2VudGVyJTIwMi5jcmwwgakG
CCsGAQUFBwEBBIGcMIGZMGEGCCsGAQUFBzAChlVodHRwOi8vdGVzdGNhLmNyeXB0
b3Byby5ydS9DZXJ0RW5yb2xsL3Rlc3QtY2EtMjAxNF9DUllQVE8tUFJPJTIwVGVz
dCUyMENlbnRlciUyMDIuY3J0MDQGCCsGAQUFBzABhihodHRwOi8vdGVzdGNhLmNy
eXB0b3Byby5ydS9vY3NwL29jc3Auc3JmMAgGBiqFAwICAwNBAA+nkIdgmqgVr/2J
FlwzT6GFy4Cv0skv+KuUyfrd7kX4jcY/oGwxpxBv5WfNYDnHrVK90bNsXTqlon2M
veFd3yM=
-----END CERTIFICATE-----
]]

function dump(t,i)
    for k,v in pairs(t) do
        if type(v) == 'table' then
            print( string.rep('\t',i),k..'={')
            dump(v,i+1)
            print( string.rep('\t',i),'}')
        elseif type(v) == 'userdata' then
            local s = tostring(v)
            if s:match('^openssl.asn1_') then
                if type(k)=='userdata' and tostring(k):match('^openssl.asn1_object') then
                    print( string.rep('\t',i),k:sn()..'='..v:data() )
                elseif s:match('^openssl.asn1_integer') then
                    print( string.rep('\t',i),tostring(k)..'='..tostring(v),v:bn())
                else
                    print( string.rep('\t',i),tostring(k)..'='..tostring(v),v:data())
                end
            elseif s:match('^openssl.x509_name') then
                print( string.rep('\t',i),k..'='..v:oneline())
                print( string.rep('\t',i),k..'={')
                dump(v:info(true), i+1)
                print( string.rep('\t',i),k..'=}')
            elseif s:match('^openssl.x509_extension') then
                print( string.rep('\t',i),k..'={')
                dump(v:info(true), i+1)
                print(string.rep('\t',i),'}')
            elseif s:match('^openssl.x509_algor') then
                print(string.rep('\t',i), k..'='..v:tostring())
            else
                print( string.rep('\t',i),k..'='..v)
            end
        else
            print( string.rep('\t',i),k..'='..tostring(v))
        end
    end
end

function test_x509()
    local x = openssl.x509.read(certasstring)
    local t = x:parse()
    dump(t,0)
end

test_x509()
