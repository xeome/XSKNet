# AF_XDP

## Kurulum

Binaryleri ve objeleri derlemek için:

```sh
git submodule update --init
make
cd src
make
```

Sonra test ortamı için [burayı](https://github.com/xdp-project/xdp-tutorial/blob/master/testenv/README.org) takip edin.

kısacası:

```sh
sudo ./testenv.sh setup --name=test
```

aliaslar için `eval $(./testenv.sh alias)`

## Kullanma

`ip a` ile test interface'ini bulun. (örneğin `test`)

```sh
sudo ./af_xdp_user -d test --filename af_xdp_kern.o -p
```

`-d test` interface belirtmek için.

`--filename af_xdp_kern.o` kernel objesi belirtmek için.

`-p` polling modu için.

başka bir terminalde:

```sh
t ping
```
