/*  Pokemon Species Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Generated from PokeAPI's pokemon, pokemon_species, pokemon_types and types
 *  tables (https://github.com/PokeAPI/pokeapi, data/v2/csv). Each entry is
 *  "slug=types|types" with one "type.type" group per form that can be met in the
 *  wild (regional forms included; Mega, Gigantamax and similar forms left out).
 *
 */

#include <map>
#include <sstream>
#include "Pokemon_SpeciesTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


static const char* SPECIES_TYPES =
    "bulbasaur=grass.poison;ivysaur=grass.poison;venusaur=grass.poison;charmander=fire;charmeleon=fire;"
    "charizard=fire.flying;squirtle=water;wartortle=water;blastoise=water;caterpie=bug;metapod=bug;"
    "butterfree=bug.flying;weedle=bug.poison;kakuna=bug.poison;beedrill=bug.poison;pidgey=normal.flying;"
    "pidgeotto=normal.flying;pidgeot=normal.flying;rattata=dark.normal|normal;"
    "raticate=dark.normal|normal;spearow=normal.flying;fearow=normal.flying;ekans=poison;arbok=poison;"
    "pikachu=electric;raichu=electric|electric.psychic;sandshrew=ground|ice.steel;"
    "sandslash=ground|ice.steel;nidoran-f=poison;nidorina=poison;nidoqueen=poison.ground;"
    "nidoran-m=poison;nidorino=poison;nidoking=poison.ground;clefairy=fairy;clefable=fairy;"
    "vulpix=fire|ice;ninetales=fire|ice.fairy;jigglypuff=normal.fairy;wigglytuff=normal.fairy;"
    "zubat=poison.flying;golbat=poison.flying;oddish=grass.poison;gloom=grass.poison;"
    "vileplume=grass.poison;paras=bug.grass;parasect=bug.grass;venonat=bug.poison;venomoth=bug.poison;"
    "diglett=ground|ground.steel;dugtrio=ground|ground.steel;meowth=dark|normal|steel;"
    "persian=dark|normal;psyduck=water;golduck=water;mankey=fighting;primeape=fighting;"
    "growlithe=fire|fire.rock;arcanine=fire|fire.rock;poliwag=water;poliwhirl=water;"
    "poliwrath=water.fighting;abra=psychic;kadabra=psychic;alakazam=psychic;machop=fighting;"
    "machoke=fighting;machamp=fighting;bellsprout=grass.poison;weepinbell=grass.poison;"
    "victreebel=grass.poison;tentacool=water.poison;tentacruel=water.poison;"
    "geodude=rock.electric|rock.ground;graveler=rock.electric|rock.ground;"
    "golem=rock.electric|rock.ground;ponyta=fire|psychic;rapidash=fire|psychic.fairy;"
    "slowpoke=psychic|water.psychic;slowbro=poison.psychic|water.psychic;magnemite=electric.steel;"
    "magneton=electric.steel;farfetch-d=normal.flying|fighting;doduo=normal.flying;dodrio=normal.flying;"
    "seel=water;dewgong=water.ice;grimer=poison|poison.dark;muk=poison|poison.dark;shellder=water;"
    "cloyster=water.ice;gastly=ghost.poison;haunter=ghost.poison;gengar=ghost.poison;onix=rock.ground;"
    "drowzee=psychic;hypno=psychic;krabby=water;kingler=water;voltorb=electric|electric.grass;"
    "electrode=electric|electric.grass;exeggcute=grass.psychic;exeggutor=grass.dragon|grass.psychic;"
    "cubone=ground;marowak=fire.ghost|ground;hitmonlee=fighting;hitmonchan=fighting;lickitung=normal;"
    "koffing=poison;weezing=poison|poison.fairy;rhyhorn=ground.rock;rhydon=ground.rock;chansey=normal;"
    "tangela=grass;kangaskhan=normal;horsea=water;seadra=water;goldeen=water;seaking=water;staryu=water;"
    "starmie=water.psychic;mr-mime=ice.psychic|psychic.fairy;scyther=bug.flying;jynx=ice.psychic;"
    "electabuzz=electric;magmar=fire;pinsir=bug;tauros=fighting|fighting.fire|fighting.water|normal;"
    "magikarp=water;gyarados=water.flying;lapras=water.ice;ditto=normal;eevee=normal;vaporeon=water;"
    "jolteon=electric;flareon=fire;porygon=normal;omanyte=rock.water;omastar=rock.water;"
    "kabuto=rock.water;kabutops=rock.water;aerodactyl=rock.flying;snorlax=normal;"
    "articuno=ice.flying|psychic.flying;zapdos=electric.flying|fighting.flying;"
    "moltres=dark.flying|fire.flying;dratini=dragon;dragonair=dragon;dragonite=dragon.flying;"
    "mewtwo=psychic;mew=psychic;chikorita=grass;bayleef=grass;meganium=grass;cyndaquil=fire;quilava=fire;"
    "typhlosion=fire|fire.ghost;totodile=water;croconaw=water;feraligatr=water;sentret=normal;"
    "furret=normal;hoothoot=normal.flying;noctowl=normal.flying;ledyba=bug.flying;ledian=bug.flying;"
    "spinarak=bug.poison;ariados=bug.poison;crobat=poison.flying;chinchou=water.electric;"
    "lanturn=water.electric;pichu=electric;cleffa=fairy;igglybuff=normal.fairy;togepi=fairy;"
    "togetic=fairy.flying;natu=psychic.flying;xatu=psychic.flying;mareep=electric;flaaffy=electric;"
    "ampharos=electric;bellossom=grass;marill=water.fairy;azumarill=water.fairy;sudowoodo=rock;"
    "politoed=water;hoppip=grass.flying;skiploom=grass.flying;jumpluff=grass.flying;aipom=normal;"
    "sunkern=grass;sunflora=grass;yanma=bug.flying;wooper=poison.ground|water.ground;"
    "quagsire=water.ground;espeon=psychic;umbreon=dark;murkrow=dark.flying;"
    "slowking=poison.psychic|water.psychic;misdreavus=ghost;unown=psychic;wobbuffet=psychic;"
    "girafarig=normal.psychic;pineco=bug;forretress=bug.steel;dunsparce=normal;gligar=ground.flying;"
    "steelix=steel.ground;snubbull=fairy;granbull=fairy;qwilfish=dark.poison|water.poison;"
    "scizor=bug.steel;shuckle=bug.rock;heracross=bug.fighting;sneasel=dark.ice|fighting.poison;"
    "teddiursa=normal;ursaring=normal;slugma=fire;magcargo=fire.rock;swinub=ice.ground;"
    "piloswine=ice.ground;corsola=ghost|water.rock;remoraid=water;octillery=water;delibird=ice.flying;"
    "mantine=water.flying;skarmory=steel.flying;houndour=dark.fire;houndoom=dark.fire;"
    "kingdra=water.dragon;phanpy=ground;donphan=ground;porygon2=normal;stantler=normal;smeargle=normal;"
    "tyrogue=fighting;hitmontop=fighting;smoochum=ice.psychic;elekid=electric;magby=fire;miltank=normal;"
    "blissey=normal;raikou=electric;entei=fire;suicune=water;larvitar=rock.ground;pupitar=rock.ground;"
    "tyranitar=rock.dark;lugia=psychic.flying;ho-oh=fire.flying;celebi=psychic.grass;treecko=grass;"
    "grovyle=grass;sceptile=grass;torchic=fire;combusken=fire.fighting;blaziken=fire.fighting;"
    "mudkip=water;marshtomp=water.ground;swampert=water.ground;poochyena=dark;mightyena=dark;"
    "zigzagoon=dark.normal|normal;linoone=dark.normal|normal;wurmple=bug;silcoon=bug;"
    "beautifly=bug.flying;cascoon=bug;dustox=bug.poison;lotad=water.grass;lombre=water.grass;"
    "ludicolo=water.grass;seedot=grass;nuzleaf=grass.dark;shiftry=grass.dark;taillow=normal.flying;"
    "swellow=normal.flying;wingull=water.flying;pelipper=water.flying;ralts=psychic.fairy;"
    "kirlia=psychic.fairy;gardevoir=psychic.fairy;surskit=bug.water;masquerain=bug.flying;"
    "shroomish=grass;breloom=grass.fighting;slakoth=normal;vigoroth=normal;slaking=normal;"
    "nincada=bug.ground;ninjask=bug.flying;shedinja=bug.ghost;whismur=normal;loudred=normal;"
    "exploud=normal;makuhita=fighting;hariyama=fighting;azurill=normal.fairy;nosepass=rock;skitty=normal;"
    "delcatty=normal;sableye=dark.ghost;mawile=steel.fairy;aron=steel.rock;lairon=steel.rock;"
    "aggron=steel.rock;meditite=fighting.psychic;medicham=fighting.psychic;electrike=electric;"
    "manectric=electric;plusle=electric;minun=electric;volbeat=bug;illumise=bug;roselia=grass.poison;"
    "gulpin=poison;swalot=poison;carvanha=water.dark;sharpedo=water.dark;wailmer=water;wailord=water;"
    "numel=fire.ground;camerupt=fire.ground;torkoal=fire;spoink=psychic;grumpig=psychic;spinda=normal;"
    "trapinch=ground;vibrava=ground.dragon;flygon=ground.dragon;cacnea=grass;cacturne=grass.dark;"
    "swablu=normal.flying;altaria=dragon.flying;zangoose=normal;seviper=poison;lunatone=rock.psychic;"
    "solrock=rock.psychic;barboach=water.ground;whiscash=water.ground;corphish=water;"
    "crawdaunt=water.dark;baltoy=ground.psychic;claydol=ground.psychic;lileep=rock.grass;"
    "cradily=rock.grass;anorith=rock.bug;armaldo=rock.bug;feebas=water;milotic=water;"
    "castform=fire|ice|normal|water;kecleon=normal;shuppet=ghost;banette=ghost;duskull=ghost;"
    "dusclops=ghost;tropius=grass.flying;chimecho=psychic;absol=dark;wynaut=psychic;snorunt=ice;"
    "glalie=ice;spheal=ice.water;sealeo=ice.water;walrein=ice.water;clamperl=water;huntail=water;"
    "gorebyss=water;relicanth=water.rock;luvdisc=water;bagon=dragon;shelgon=dragon;"
    "salamence=dragon.flying;beldum=steel.psychic;metang=steel.psychic;metagross=steel.psychic;"
    "regirock=rock;regice=ice;registeel=steel;latias=dragon.psychic;latios=dragon.psychic;kyogre=water;"
    "groudon=ground;rayquaza=dragon.flying;jirachi=steel.psychic;deoxys=psychic;turtwig=grass;"
    "grotle=grass;torterra=grass.ground;chimchar=fire;monferno=fire.fighting;infernape=fire.fighting;"
    "piplup=water;prinplup=water;empoleon=water.steel;starly=normal.flying;staravia=normal.flying;"
    "staraptor=normal.flying;bidoof=normal;bibarel=normal.water;kricketot=bug;kricketune=bug;"
    "shinx=electric;luxio=electric;luxray=electric;budew=grass.poison;roserade=grass.poison;"
    "cranidos=rock;rampardos=rock;shieldon=rock.steel;bastiodon=rock.steel;burmy=bug;"
    "wormadam=bug.grass|bug.ground|bug.steel;mothim=bug.flying;combee=bug.flying;vespiquen=bug.flying;"
    "pachirisu=electric;buizel=water;floatzel=water;cherubi=grass;cherrim=grass;shellos=water;"
    "gastrodon=water.ground;ambipom=normal;drifloon=ghost.flying;drifblim=ghost.flying;buneary=normal;"
    "lopunny=normal;mismagius=ghost;honchkrow=dark.flying;glameow=normal;purugly=normal;"
    "chingling=psychic;stunky=poison.dark;skuntank=poison.dark;bronzor=steel.psychic;"
    "bronzong=steel.psychic;bonsly=rock;mime-jr=psychic.fairy;happiny=normal;chatot=normal.flying;"
    "spiritomb=ghost.dark;gible=dragon.ground;gabite=dragon.ground;garchomp=dragon.ground;"
    "munchlax=normal;riolu=fighting;lucario=fighting.steel;hippopotas=ground;hippowdon=ground;"
    "skorupi=poison.bug;drapion=poison.dark;croagunk=poison.fighting;toxicroak=poison.fighting;"
    "carnivine=grass;finneon=water;lumineon=water;mantyke=water.flying;snover=grass.ice;"
    "abomasnow=grass.ice;weavile=dark.ice;magnezone=electric.steel;lickilicky=normal;"
    "rhyperior=ground.rock;tangrowth=grass;electivire=electric;magmortar=fire;togekiss=fairy.flying;"
    "yanmega=bug.flying;leafeon=grass;glaceon=ice;gliscor=ground.flying;mamoswine=ice.ground;"
    "porygon-z=normal;gallade=psychic.fighting;probopass=rock.steel;dusknoir=ghost;froslass=ice.ghost;"
    "rotom=electric.fire|electric.flying|electric.ghost|electric.grass|electric.ice|electric.water;"
    "uxie=psychic;mesprit=psychic;azelf=psychic;dialga=steel.dragon;palkia=water.dragon;"
    "heatran=fire.steel;regigigas=normal;giratina=ghost.dragon;cresselia=psychic;phione=water;"
    "manaphy=water;darkrai=dark;shaymin=grass|grass.flying;arceus=normal;victini=psychic.fire;"
    "snivy=grass;servine=grass;serperior=grass;tepig=fire;pignite=fire.fighting;emboar=fire.fighting;"
    "oshawott=water;dewott=water;samurott=water|water.dark;patrat=normal;watchog=normal;lillipup=normal;"
    "herdier=normal;stoutland=normal;purrloin=dark;liepard=dark;pansage=grass;simisage=grass;"
    "pansear=fire;simisear=fire;panpour=water;simipour=water;munna=psychic;musharna=psychic;"
    "pidove=normal.flying;tranquill=normal.flying;unfezant=normal.flying;blitzle=electric;"
    "zebstrika=electric;roggenrola=rock;boldore=rock;gigalith=rock;woobat=psychic.flying;"
    "swoobat=psychic.flying;drilbur=ground;excadrill=ground.steel;audino=normal;timburr=fighting;"
    "gurdurr=fighting;conkeldurr=fighting;tympole=water;palpitoad=water.ground;seismitoad=water.ground;"
    "throh=fighting;sawk=fighting;sewaddle=bug.grass;swadloon=bug.grass;leavanny=bug.grass;"
    "venipede=bug.poison;whirlipede=bug.poison;scolipede=bug.poison;cottonee=grass.fairy;"
    "whimsicott=grass.fairy;petilil=grass;lilligant=grass|grass.fighting;basculin=water;"
    "sandile=ground.dark;krokorok=ground.dark;krookodile=ground.dark;darumaka=fire|ice;"
    "darmanitan=fire|fire.psychic|ice|ice.fire;maractus=grass;dwebble=bug.rock;crustle=bug.rock;"
    "scraggy=dark.fighting;scrafty=dark.fighting;sigilyph=psychic.flying;yamask=ghost|ground.ghost;"
    "cofagrigus=ghost;tirtouga=water.rock;carracosta=water.rock;archen=rock.flying;archeops=rock.flying;"
    "trubbish=poison;garbodor=poison;zorua=dark|normal.ghost;zoroark=dark|normal.ghost;minccino=normal;"
    "cinccino=normal;gothita=psychic;gothorita=psychic;gothitelle=psychic;solosis=psychic;"
    "duosion=psychic;reuniclus=psychic;ducklett=water.flying;swanna=water.flying;vanillite=ice;"
    "vanillish=ice;vanilluxe=ice;deerling=normal.grass;sawsbuck=normal.grass;emolga=electric.flying;"
    "karrablast=bug;escavalier=bug.steel;foongus=grass.poison;amoonguss=grass.poison;"
    "frillish=water.ghost;jellicent=water.ghost;alomomola=water;joltik=bug.electric;"
    "galvantula=bug.electric;ferroseed=grass.steel;ferrothorn=grass.steel;klink=steel;klang=steel;"
    "klinklang=steel;tynamo=electric;eelektrik=electric;eelektross=electric;elgyem=psychic;"
    "beheeyem=psychic;litwick=ghost.fire;lampent=ghost.fire;chandelure=ghost.fire;axew=dragon;"
    "fraxure=dragon;haxorus=dragon;cubchoo=ice;beartic=ice;cryogonal=ice;shelmet=bug;accelgor=bug;"
    "stunfisk=ground.electric|ground.steel;mienfoo=fighting;mienshao=fighting;druddigon=dragon;"
    "golett=ground.ghost;golurk=ground.ghost;pawniard=dark.steel;bisharp=dark.steel;bouffalant=normal;"
    "rufflet=normal.flying;braviary=normal.flying|psychic.flying;vullaby=dark.flying;"
    "mandibuzz=dark.flying;heatmor=fire;durant=bug.steel;deino=dark.dragon;zweilous=dark.dragon;"
    "hydreigon=dark.dragon;larvesta=bug.fire;volcarona=bug.fire;cobalion=steel.fighting;"
    "terrakion=rock.fighting;virizion=grass.fighting;tornadus=flying;thundurus=electric.flying;"
    "reshiram=dragon.fire;zekrom=dragon.electric;landorus=ground.flying;kyurem=dragon.ice;"
    "keldeo=water.fighting;meloetta=normal.fighting|normal.psychic;genesect=bug.steel;chespin=grass;"
    "quilladin=grass;chesnaught=grass.fighting;fennekin=fire;braixen=fire;delphox=fire.psychic;"
    "froakie=water;frogadier=water;greninja=water.dark;bunnelby=normal;diggersby=normal.ground;"
    "fletchling=normal.flying;fletchinder=fire.flying;talonflame=fire.flying;scatterbug=bug;spewpa=bug;"
    "vivillon=bug.flying;litleo=fire.normal;pyroar=fire.normal;flabebe=fairy;floette=fairy;florges=fairy;"
    "skiddo=grass;gogoat=grass;pancham=fighting;pangoro=fighting.dark;furfrou=normal;espurr=psychic;"
    "meowstic=psychic;honedge=steel.ghost;doublade=steel.ghost;aegislash=steel.ghost;spritzee=fairy;"
    "aromatisse=fairy;swirlix=fairy;slurpuff=fairy;inkay=dark.psychic;malamar=dark.psychic;"
    "binacle=rock.water;barbaracle=rock.water;skrelp=poison.water;dragalge=poison.dragon;clauncher=water;"
    "clawitzer=water;helioptile=electric.normal;heliolisk=electric.normal;tyrunt=rock.dragon;"
    "tyrantrum=rock.dragon;amaura=rock.ice;aurorus=rock.ice;sylveon=fairy;hawlucha=fighting.flying;"
    "dedenne=electric.fairy;carbink=rock.fairy;goomy=dragon;sliggoo=dragon|steel.dragon;"
    "goodra=dragon|steel.dragon;klefki=steel.fairy;phantump=ghost.grass;trevenant=ghost.grass;"
    "pumpkaboo=ghost.grass;gourgeist=ghost.grass;bergmite=ice;avalugg=ice|ice.rock;noibat=flying.dragon;"
    "noivern=flying.dragon;xerneas=fairy;yveltal=dark.flying;zygarde=dragon.ground;diancie=rock.fairy;"
    "hoopa=psychic.dark|psychic.ghost;volcanion=fire.water;rowlet=grass.flying;dartrix=grass.flying;"
    "decidueye=grass.fighting|grass.ghost;litten=fire;torracat=fire;incineroar=fire.dark;popplio=water;"
    "brionne=water;primarina=water.fairy;pikipek=normal.flying;trumbeak=normal.flying;"
    "toucannon=normal.flying;yungoos=normal;gumshoos=normal;grubbin=bug;charjabug=bug.electric;"
    "vikavolt=bug.electric;crabrawler=fighting;crabominable=fighting.ice;"
    "oricorio=electric.flying|fire.flying|ghost.flying|psychic.flying;cutiefly=bug.fairy;"
    "ribombee=bug.fairy;rockruff=rock;lycanroc=rock;wishiwashi=water;mareanie=poison.water;"
    "toxapex=poison.water;mudbray=ground;mudsdale=ground;dewpider=water.bug;araquanid=water.bug;"
    "fomantis=grass;lurantis=grass;morelull=grass.fairy;shiinotic=grass.fairy;salandit=poison.fire;"
    "salazzle=poison.fire;stufful=normal.fighting;bewear=normal.fighting;bounsweet=grass;steenee=grass;"
    "tsareena=grass;comfey=fairy;oranguru=normal.psychic;passimian=fighting;wimpod=bug.water;"
    "golisopod=bug.water;sandygast=ghost.ground;palossand=ghost.ground;pyukumuku=water;type-null=normal;"
    "silvally=normal;minior=rock.flying;komala=normal;turtonator=fire.dragon;togedemaru=electric.steel;"
    "mimikyu=ghost.fairy;bruxish=water.psychic;drampa=normal.dragon;dhelmise=ghost.grass;jangmo-o=dragon;"
    "hakamo-o=dragon.fighting;kommo-o=dragon.fighting;tapu-koko=electric.fairy;tapu-lele=psychic.fairy;"
    "tapu-bulu=grass.fairy;tapu-fini=water.fairy;cosmog=psychic;cosmoem=psychic;solgaleo=psychic.steel;"
    "lunala=psychic.ghost;nihilego=rock.poison;buzzwole=bug.fighting;pheromosa=bug.fighting;"
    "xurkitree=electric;celesteela=steel.flying;kartana=grass.steel;guzzlord=dark.dragon;"
    "necrozma=psychic|psychic.dragon|psychic.ghost|psychic.steel;magearna=steel.fairy;"
    "marshadow=fighting.ghost;poipole=poison;naganadel=poison.dragon;stakataka=rock.steel;"
    "blacephalon=fire.ghost;zeraora=electric;meltan=steel;melmetal=steel;grookey=grass;thwackey=grass;"
    "rillaboom=grass;scorbunny=fire;raboot=fire;cinderace=fire;sobble=water;drizzile=water;"
    "inteleon=water;skwovet=normal;greedent=normal;rookidee=flying;corvisquire=flying;"
    "corviknight=flying.steel;blipbug=bug;dottler=bug.psychic;orbeetle=bug.psychic;nickit=dark;"
    "thievul=dark;gossifleur=grass;eldegoss=grass;wooloo=normal;dubwool=normal;chewtle=water;"
    "drednaw=water.rock;yamper=electric;boltund=electric;rolycoly=rock;carkol=rock.fire;"
    "coalossal=rock.fire;applin=grass.dragon;flapple=grass.dragon;appletun=grass.dragon;silicobra=ground;"
    "sandaconda=ground;cramorant=flying.water;arrokuda=water;barraskewda=water;toxel=electric.poison;"
    "toxtricity=electric.poison;sizzlipede=fire.bug;centiskorch=fire.bug;clobbopus=fighting;"
    "grapploct=fighting;sinistea=ghost;polteageist=ghost;hatenna=psychic;hattrem=psychic;"
    "hatterene=psychic.fairy;impidimp=dark.fairy;morgrem=dark.fairy;grimmsnarl=dark.fairy;"
    "obstagoon=dark.normal;perrserker=steel;cursola=ghost;sirfetch-d=fighting;mr-rime=ice.psychic;"
    "runerigus=ground.ghost;milcery=fairy;alcremie=fairy;falinks=fighting;pincurchin=electric;"
    "snom=ice.bug;frosmoth=ice.bug;stonjourner=rock;eiscue=ice;indeedee=psychic.normal;"
    "morpeko=electric.dark;cufant=steel;copperajah=steel;dracozolt=electric.dragon;"
    "arctozolt=electric.ice;dracovish=water.dragon;arctovish=water.ice;duraludon=steel.dragon;"
    "dreepy=dragon.ghost;drakloak=dragon.ghost;dragapult=dragon.ghost;zacian=fairy|fairy.steel;"
    "zamazenta=fighting|fighting.steel;eternatus=poison.dragon;kubfu=fighting;"
    "urshifu=fighting.dark|fighting.water;zarude=dark.grass;regieleki=electric;regidrago=dragon;"
    "glastrier=ice;spectrier=ghost;calyrex=psychic.ghost|psychic.grass|psychic.ice;"
    "wyrdeer=normal.psychic;kleavor=bug.rock;ursaluna=ground.normal;basculegion=water.ghost;"
    "sneasler=fighting.poison;overqwil=dark.poison;enamorus=fairy.flying;sprigatito=grass;"
    "floragato=grass;meowscarada=grass.dark;fuecoco=fire;crocalor=fire;skeledirge=fire.ghost;"
    "quaxly=water;quaxwell=water;quaquaval=water.fighting;lechonk=normal;oinkologne=normal;"
    "tarountula=bug;spidops=bug;nymble=bug;lokix=bug.dark;pawmi=electric;pawmo=electric.fighting;"
    "pawmot=electric.fighting;tandemaus=normal;maushold=normal;fidough=fairy;dachsbun=fairy;"
    "smoliv=grass.normal;dolliv=grass.normal;arboliva=grass.normal;squawkabilly=normal.flying;nacli=rock;"
    "naclstack=rock;garganacl=rock;charcadet=fire;armarouge=fire.psychic;ceruledge=fire.ghost;"
    "tadbulb=electric;bellibolt=electric;wattrel=electric.flying;kilowattrel=electric.flying;"
    "maschiff=dark;mabosstiff=dark;shroodle=poison.normal;grafaiai=poison.normal;bramblin=grass.ghost;"
    "brambleghast=grass.ghost;toedscool=ground.grass;toedscruel=ground.grass;klawf=rock;capsakid=grass;"
    "scovillain=grass.fire;rellor=bug;rabsca=bug.psychic;flittle=psychic;espathra=psychic;"
    "tinkatink=fairy.steel;tinkatuff=fairy.steel;tinkaton=fairy.steel;wiglett=water;wugtrio=water;"
    "bombirdier=flying.dark;finizen=water;palafin=water;varoom=steel.poison;revavroom=steel.poison;"
    "cyclizar=dragon.normal;orthworm=steel;glimmet=rock.poison;glimmora=rock.poison;greavard=ghost;"
    "houndstone=ghost;flamigo=flying.fighting;cetoddle=ice;cetitan=ice;veluza=water.psychic;"
    "dondozo=water;tatsugiri=dragon.water;annihilape=fighting.ghost;clodsire=poison.ground;"
    "farigiraf=normal.psychic;dudunsparce=normal;kingambit=dark.steel;great-tusk=ground.fighting;"
    "scream-tail=fairy.psychic;brute-bonnet=grass.dark;flutter-mane=ghost.fairy;"
    "slither-wing=bug.fighting;sandy-shocks=electric.ground;iron-treads=ground.steel;"
    "iron-bundle=ice.water;iron-hands=fighting.electric;iron-jugulis=dark.flying;iron-moth=fire.poison;"
    "iron-thorns=rock.electric;frigibax=dragon.ice;arctibax=dragon.ice;baxcalibur=dragon.ice;"
    "gimmighoul=ghost;gholdengo=steel.ghost;wo-chien=dark.grass;chien-pao=dark.ice;ting-lu=dark.ground;"
    "chi-yu=dark.fire;roaring-moon=dragon.dark;iron-valiant=fairy.fighting;koraidon=fighting.dragon;"
    "miraidon=electric.dragon;walking-wake=water.dragon;iron-leaves=grass.psychic;dipplin=grass.dragon;"
    "poltchageist=grass.ghost;sinistcha=grass.ghost;okidogi=poison.fighting;munkidori=poison.psychic;"
    "fezandipiti=poison.fairy;ogerpon=grass|grass.fire|grass.rock|grass.water;archaludon=steel.dragon;"
    "hydrapple=grass.dragon;gouging-fire=fire.dragon;raging-bolt=electric.dragon;"
    "iron-boulder=rock.psychic;iron-crown=steel.psychic;terapagos=normal;pecharunt=poison.ghost;";

static std::map<std::string, std::vector<std::set<std::string>>> parse_species_types(){
    std::map<std::string, std::vector<std::set<std::string>>> ret;
    std::stringstream entries(SPECIES_TYPES);
    std::string entry;
    while (std::getline(entries, entry, ';')){
        const size_t equals = entry.find('=');
        if (equals == std::string::npos){
            continue;
        }
        std::vector<std::set<std::string>>& forms = ret[entry.substr(0, equals)];
        std::stringstream groups(entry.substr(equals + 1));
        std::string group;
        while (std::getline(groups, group, '|')){
            std::set<std::string> form;
            std::stringstream types(group);
            std::string type;
            while (std::getline(types, type, '.')){
                form.insert(type);
            }
            forms.push_back(std::move(form));
        }
    }
    return ret;
}

const std::vector<std::set<std::string>>* species_form_types(const std::string& slug){
    static const std::map<std::string, std::vector<std::set<std::string>>> table = parse_species_types();
    auto iter = table.find(slug);
    return iter == table.end() ? nullptr : &iter->second;
}


}
}
