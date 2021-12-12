# Redes de Computadores - Ligação de dados

## Sumário
Este projeto foi desenvolvido no âmbito da unidade curricular de Redes de Computadores e visa a implemetação de um protocolo de ligação de dados e testando-o com uma aplicação de transferência de ficheiros.

## 1. Introdução
Existem inúmeras motivações para que existam mecanismos de transferência de dados entre computadores diferentes, por exemplo, para comunicar à distância. Além disso, é fundamental que essa transferência de dados decorra sem qualquer tipo de erros e de uma forma confiável - esta é, indubitavelmente, a principal motivação para a realização deste pequeno projeto. 

Neste primeiro trabalho prático foi-nos proposto a implementação de um protocolo para a troca de dados entre 2 computadores ligados por uma porta série. As principais tecnologias utilizadas foram a linguagem C, a porta série RS-232 e ainda a *API* programática do *Linux*.

## 2. Arquitetura e Estrutura de código
A impelmentação do protocolo pode ser dividido em diferentes unidades lógicas cada uma independente entre si. Deste modo, temos um protocolo para a aplicação onde uma das partes, isto é o emissor comunica com o recetor usando uma interface que oferece uma abstração à camada de ligação de dados entre os dois programas.

Como foi expresso no parágrafo anterior, o código encontra-se divido de modo a proporcionar diferentes camadas de abstração, isto significa que as diferentes unidades lógicas são independentes entre si. No nosso caso, essa independência é garantida com recurso à disposição do código em diferentes ficheiros - sobretudo de *header files*, mas também com o uso da *keyword* `static` nas declarações das funções que são internas a uma determinada unidade lógica, para que só aí possam ser utilizadas e, simultaneamente, estar escondidas do restante código.

No que concerne à estrutura dos ficheiros, esta é muito simples. Os ficheiros `protocol.h` e `protocol.c` representam a camada de ligação de dados, depois os ficheiros `sender.c` e `receiver.c` representam a camada da aplicação e, finalmente, os ficheiros `utils.h` e `utils.c` que contêm as definições das funções utilitárias.

Para utilizar os 2 programas basta executar um dos seguintes comandos, de acordo com o fluxo de transmissão, em cada um dos dispositivos:

* Para o recetor
``` 
$ recv <num. da porta> <nome do ficheiro a receber>
```

* Para o recetor
```
$ sndr <num. da porta> <nome do ficheiro a enviar>
```

## 3. Protocolo de ligação de dados
De acordo com o enunciado proposto, devem ser implementadas 4 funções que formam uma *API* a ser usada pelas aplicações, quer do emissor, quer do recetor. Eis os cabeçalhos dessa *API*:

```c 
int llopen(int port, const uint8_t addr);
ssize_t llwrite(int fd, uint8_t *buffer, ssize_t len);
ssize_t llread(int fd, uint8_t *buffer);
int llclose(int fd);
```

### 3.1 `int llopen(int port, const uint8_t addr)`
Abre o canal de comunicações fornecendo o respetivo identificador. A aplicação deve fornecer o número associado à porta série e ainda um valor de modo a identificar de que "lado" da ligação se encontra. Os valores possíveis são `RECEIVER` e `TRANSMITTER` e estão definidos no ficheiro `protocol.h`:

```c
#define RECEIVER 0x01
#define TRANSMITTER 0x03
```

### 3.2 `ssize_t llwrite(int fd, uint8_t *buffer, ssize_t len)`
Escreve os dados contidos no `buffer` no canal de comunicações. Retorna o número de *bytes* escritos no canal, ou então um valor negativo em caso de erro.

### 3.3 `ssize_t llread(int fd, uint8_t *buffer)`
Lê os dados disponíveis no canal de comunicações, escrevendo-os no `buffer` passado como argumento. Retorna o valor de *bytes* lidos, ou então um valor negativo em caso de erro.

### 3.4 `int llclose(int fd)`
Fecha o canal de comunicações.

### 3.5 Opções
O protocolo permite que se configurem algumas opções (em tempo de compilação) a partir do ficheiro `makefile`, são elas:

| Opção | Descrição |
| --- | ----------- |
| `BAUDRATE` | Número de símbolo que fluem no canal de comunicações por segundo. |
| `TOUT` | Número de segundos de espera, no emissor, sem uma resposta do recetor até se desencadear uma retransmissão. |
| `TPROP` | Número de segundos de espera no recetor de modo a simular um atraso no [tempo de propagação](#estatisticas) de uma trama. |
| `MAX_RETRIES` | Número máximo de tentativas de retransmissão até que o emissor desista de retransmitir. |
| `MAX_PACKET_SIZE` | Tamanho máximo, em *bytes*, para os pacotes da aplicação |

### 3.6 Detalhes de implementação
Na implementação do protocolo da ligação de dados os principais desafios foram as implementações dos mecanismos de transparência e deteção de erros nos dados transmitidos e do mecanismo de leitura de dados, sobretudo por causa da panóplia de nuances a ter em conta.

O fluxo de execução é bastante simples, com a característica de que na nossa implementação é o emissor quem toma a iniciativa. Deste modo, o emissor começa por enviar o comando `SET` ficando logo de seguida à espera de uma resposta do recetor. Já do lado do recetor, o programa aguarda pela receção da trama `SET` e envia a resposta - uma trama do tipo `UA`.

O envio das tramas de supervisão é feito pela função `send_frame_us(int fd, uint8_t cmd, uint8_t addr)` onde `fd` descreve o indentificador do canal de comunicações, `cmd` o valor a ser enviado no campo de comando e `addr` que descreve quem envia a trama. Os valores possíveis para `addr` são os mesmos que os da função [`llopen`](#llopen). Nos mesmo moldes, para a `cmd` os valores possíveis são:

```c 
typedef enum { SET, DISC, UA, RR_0, REJ_0, RR_1, REJ_1 } frameCmd;
```

A construção das tramas de supervisão fica clara com o seguinte excerto de código:

```c
unsigned char frame[5];
frame[0] = frame[4] = FLAG;
frame[1] = addr;
frame[2] = cmds[cmd];
frame[3] = frame[1] ^ frame[2];
```

Por outro lado, a receção das tramas de supervisão (e de informação) é digerida na função `read_frame_us(int fd, const uint8_t cmd_mask, const uint8_t addr)`. Esta função é mais complexa que a anterior, na medida em que existe uma máquina de estados para intrepertar cada *byte* de informação lido - isto acontece porque há a necessidade de se ler os dados que chegam *byte* a *byte*. Aqui, os parâmetros, apesar de terem nomes semelhantes, tomam uma intrepertação ligeiramente diferente. Assim, `fd` é o identificador do canal de comunicações, `cmd_mask` é uma máscara de *bits* para permitir que com a mesma função seja possível ler um valor dentro um conjunto valores que possam ocorrer - isto prova-se útil quando existem múltiplas possibilidades de resposta ao envio de uma trama de informação - por último, o valor `addr` representa o valor do lado que enviou a trama.

Depois, o envio e a codificação das tramas de informação é feito pelas funções `write_data` e `encode_data` chamadas por [`llwrite`](#llwrite). No outro lado da comunicação, em [`llread`](#llread), temos a leitura que é intrepertada com recurso a máquina de estado - muito semelhante à presente em `send_frame_us` - e a descodificação que é da responsabilidade da função `decode_data` No fim, após o envio de todos os dados, a conexão é terminada com a chamada a [`llclose`](#llclose).

Alguns excertos de código relevantes são os seguintes:

* As funções `encode_data` e `decode_data` que implementam o mecanismo de transparência de dados, muito importante, na medida em que permite que valores com significado especial possam ocorrer ao longo da informação trasmitida.
```c 
static ssize_t
encode_data(uint8_t **dest, const uint8_t *src, ssize_t len)
{
        ssize_t i, j;
        uint8_t bcc = src[0];
        for (i = 1; i < len; i++)
                bcc ^= src[i];

        ssize_t inc = 0;
        for (i = 0; i < len; i++)
                inc += ESCAPED_BYTE(src[i]);

        ssize_t nlen = len + inc + ESCAPED_BYTE(bcc) + 1;
        *dest = (uint8_t *)malloc(nlen);
        passert(dest != NULL, "protocol.c :: malloc", -1);

        for (i = 0, j = 0; j < len; i += ESCAPED_BYTE(src[j]) + 1, j++)
                encode_cpy(*dest, i, src[j]);
        encode_cpy(*dest, len + inc, bcc);

        return nlen;
}

static ssize_t
decode_data(uint8_t *dest, const uint8_t *src, ssize_t len)
{
        ssize_t i, j;
        ssize_t dec = 0;
        for (i = 0; i < len; i++)
                dec += IS_ESCAPE(src[i]);

        for (i = 0, j = 0; j < len - dec; i++, j++)
            dest[j] = IS_ESCAPE(src[i]) ? (src[++i] ^ KEY) : src[i];

        return len - dec;
}
```

* A função `recv_send_response` que averigua se o campo de proteção de dados está correto e que envia a resposta mais adequada ao emissor. Esta função é chamada por [`llread`](#llread).
```c
static int
recv_send_response(int fd, const uint8_t *buffer, const ssize_t len)
{
        ssize_t i;
        uint8_t bcc = buffer[0], expect_bcc = buffer[len-1];
        for (i = 1; i < len - 1; i++)
                bcc ^= buffer[i];

        uint8_t cmd;
        cmd = sequence_number ? RR_1 : RR_0;
        if (bcc != expect_bcc)
                cmd = sequence_number ? REJ_1 : REJ_0;

        send_frame_us(fd, cmd, RECEIVER);
        return (bcc == expect_bcc) ? len : -1;
}
```

## 4. Protocolo de aplicação
Como vimos na secção anterior, o protocolo da ligação de dados carateriza-se por estar mais a baixo no modelo *OSI* do que o protocolo da aplicação. Este protocolo é mais simples e recorre à *API* descrita em cima para transferir dados. 

No nosso caso implementamos 2 aplicações que representam o recetor e o transmissor dos dados. Em ambos os programas a primeira ação a ser efetuada é a abertura do canal de comunicações com a chamada a [`llopen`](#llopen). Depois, ocorre uma divergência na lógica dos 2 programas. Comecemos pelo emissor, que envia um primeiro pacote de controlo com o valor `START` no campo de controlo e o tamanho do ficheiro, depois lê pequenos fragmentos do ficheiro fornecido como argumento e envia os respetivos pacotes de dados finalizando com um pacote de controlo semelhante ao primeiro exceto no campo de controlo onde o valor é `STOP`. Este envio dos dados acontece com recurso a chamadas a [`llwrite`](#llwrite). Enquanto isso, do outro lado, o recetor vai lendo os pacotes de controlo e de informação e escrevendo-os no ficheiro fornecido como argumento do programa. Findo todo o processo de transmissão ambos os programas programas chamam a função [`llclose`](#llclose), libertam os recursos sobre a sua alçada e cessam a sua execução.

## 5. Validação 

Para a validação do protocolo impelmentado foram executados vários testes e depois verificadas as *checksums* dos ficheiros para garantir que todos os componentes do protocolo, sobretudo os mecanismos de deteção de erros, de retransmissão e de transparência funcionavam corretamente. O tipo de testes realizados foram:

* Execução com ficheiros diferentes;
* Execução "normal" com e sem introdução de erros;
* Começo da execução tardio no lado do recetor;
* Execução com interrupções na porta série.

O *output* da execução dos teste realizados foi o seguinte:

```sh 
$ recv 11 pingu.gif
```
```sh 
$ sndr 10 pinguim.gif
```
```sh 
$ sha256sum pinguim.gif pingu.gif
54da34fa5529f96c60aead3681e5ed2a53b98ce4281e62702ca2f39530c07365  pinguim.gif
54da34fa5529f96c60aead3681e5ed2a53b98ce4281e62702ca2f39530c07365  pingu.gif
```

As *checksums* foram, para todos os testes realizados, exatamente iguais, portanto o ficheiro enviado e o ficheiro recebido são exatamente iguais - o resultado pretendido. Ou seja, o protocolo é capaz de ultrapassar erros que possam ocorrer em qualquer um dos lados do eixo de comunicações. 

## 6. Eficiência de protocolo de ligação

Segundo a definição, a eficiência de um protocolo é a razão de tempo gasto entre o envio ou leitura de dados e o tempo gasto entre a espera pelas confirmações.

### 6.1 Aspetos de implemetação relativas a *ARQ* (*Automatic Repeate reQuest*)

O protcolo implementado carateriza-se pelo facto de ter a funcionalidade *ARQ*, neste caso em particular estamos perante um caso especial de *Go back N* onde $$ N=1 $$ 
Isto é, *Stop & Wait* - o emissor não deve avançar sem antes aguardar por uma resposta do recetor, seja ela uma resposta positiva ou uma rejeição devido a erros. Além disso, para *Go Back N* existe a necessidade de haver um número de sequência, como acontece na nossa implementação com a variável `sequence_number` definida no ficheiro `protocol.c`, e que permita ordenar as tramas de acordo com a ordem pretendida. Para *Stop & Wait* essa variável apenas precisa de alternar entre `0` e `1`, visto que ocorre sempre a retransmissão para uma trama que ainda não tenha sido aceite.

Contudo, a facilidade de implementação de um sistema *Stop & Wait* impede que este faça frente à eficiência de outros mecanismos, como é o caso do *selective repeat* - onde o envio de dados prossegue mesmo em caso de erro (erros que são corrigidos alguns envios depois). 

### 6.2 Caraterização estatística da eficiência do protocolo

Deste modo, os valores de eficiência para *Stop & Wait* são dados pelas seguintes fórmulas, disponíveis nos diapositivos apresentados nas aulas teóricas:

![](https://render.githubusercontent.com/render/math?math=%5Cdisplaystyle+%5Cbegin%7Balign%2A%7D%0Aa+%3D+%5Cfrac%7BT_%7Bprop%7D%7D%7BT_f%7D%0A%5Cend%7Balign%2A%7D%0A "Razão entre o tempo de propagação e o tempo de envio dos dados de um trama")<br>
![](https://render.githubusercontent.com/render/math?math=%5Cdisplaystyle+S+%3D+%5Cfrac%7BT_f%7D%7BT_f%2B2T_%7Bprop%7D%7D+%3D+%5Cfrac%7B1%7D%7B1%2B2a%7D%0A "Eficiência do protocolo sem quaisquer erros")<br>
![](https://render.githubusercontent.com/render/math?math=%5Cdisplaystyle+S_%7Be%7D+%3D+%5Cfrac%7BT_f%7D%7BE%5BA%5D%28T_f%2B2T_%7Bprop%7D%29%7D+%3D+%5Cfrac%7B1%7D%7BE%5BA%5D%281%2B2a%29%7D+%3D+%5Cfrac%7B1-FER%7D%7B1%2B2a%7D%0A "Eficiência do protocolo com erros")

Onde:

* *Tf*: tempo entre envio de dados de uma trama;
* *Tprop*: tempo de propagação de uma trama ao longo do canal de comunicações;
* *FER*: probabilidade de erro de uma trama (*Frame Error Ratio*);
* *E[A]*: número médio de tentativas para se transmitir uma trama com sucesso.

Como se observa, surgem várias conclusões. A primeira é a de que se o valor de *a* for elevado, então, a eficiência será baixa. O principal motivo para que isto ocorra pode ser a distância entre os pontos de comunicação, bem como, o facto do tamanho da trama de informação não ser suficientemente grande - o que conduz a um tempo de envio menor, e consequentemente a um valor de *a* maior. Já a segunda conclusão a que chegamos é a de que se a probabilidade de uma trama conter erros - *FER* - for elevada, naturalmente, a eficiência do protocolo irá cair. A modelação dos valores da eficiência de acordo com a probabilidade de erro de uma trama pode ser observada no gráfico seguinte:

![](./doc/efficiency.png "Valor da eficiência de acordo com o FER")

Neste gráfico, importa referir que o cenário representado pela linha púrpura é hipotético, na medida, em que todas as tramas possuem erros o que impossibilita a transferência da informação, resultado, obviamente, numa eficiência nula e constante. Por outro lado, percebe-se, pela análise do gráfico, que o valor de *a* tem a sua influência indepedentemente do valor de *FER*. Não obstante, nota-se também que para valores baixos de *a*, a eficiência depende praticamente do *FER*.

Tendo tudo isto em conta, a escolha de *Stop & Wait* para mecanismo de *ARQ* deve ser pensada, sobretudo, de acordo com a distância entre o emissor e o recetor, mesmo que seja mais fácil de ser implementado ou que o canal tenha uma capacidade elevada e com pouca probabilidade de erros.

### 6.3 Performance

O gráfico seguinte mostra os tempos de envio do ficheiro fornecido `pinguim.gif` de acordo com o tamanho máximo para um pacote de dados da aplicação. Nota que este valor pode ser alterado nas [opções](#opcoes) do ficheiro `makefile`.

![](./doc/benchmarks.png "Tempos de envio de acordo com o tamanho dos pacotes")

Como se observa, existe um valor mínimo para os tempos de envio que ronda os 256 *bytes*. Podemos então assim concluir que se para pacotes mais pequenos o número de fragmentos a enviar causa um acréscimo ao tempo de envio, por outro lado, para pacotes maiores, o esforço de processamento abafa a suposta rapidez obtida de um menor número de envios de fragmentos. 

## 7. Conclusões

Este foi um trabalho que certamente gerou um certo interesse da maioria dos alunos, sobretudo pelo facto de poderem observar fisicamente a transferência de ficheiros entre os 2 computadores no laboratório. Todavia, mesmo sendo um trabalho exigente é ótimo que assim o seja, pois obriga os estudantes a estarem a par dos conceitos teóricos leccionados nas aulas. 

Agora, em retrospetiva, verificamos que com este pequeno projeto foi possível cimentar os conhecimentos prévios em C mas também descobrir, como efetivamente, a informação era transmitida por uma porta série, bem antes da internet dar os seus primeiros passos e revolucionar essa transferência da informação.

---

* [Miguel Boaventura Rodrigues](mailto:up201906042@edu.fe.up.pt)
* [Nuno Miguel Paiva de Melo e Castro](mailto:up202003324@edu.fe.up.pt)

