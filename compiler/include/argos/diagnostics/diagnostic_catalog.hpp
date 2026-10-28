#pragma once

#include "argos/diagnostics/diagnostic_definition.hpp"

#include <array>

namespace argos::diagnostics::catalog {

namespace lexer {
inline constexpr DiagnosticDefinition unterminated_block_comment{
    "ARGOS-L0001", Severity::error, "comentário de bloco não terminado",
    "Um comentário iniciado por /* chegou ao fim do arquivo sem */.",
    "Feche o comentário com */."};
inline constexpr DiagnosticDefinition invalid_token{
    "ARGOS-L0002", Severity::error, "token inválido",
    "O caractere ou literal não forma um token válido na ArgosLang.",
    "Corrija ou remova o trecho indicado."};
inline constexpr DiagnosticDefinition invalid_utf8{
    "ARGOS-L0003", Severity::error, "sequência UTF-8 inválida",
    "O arquivo-fonte contém bytes que não representam texto UTF-8 válido.",
    "Salve o arquivo como UTF-8."};
} // namespace lexer

namespace parser {
inline constexpr DiagnosticDefinition unexpected_syntax{
    "ARGOS-P0001", Severity::error, "sintaxe inesperada",
    "A sequência de tokens não corresponde à gramática esperada neste ponto.",
    "Leia a mensagem detalhada e corrija o token indicado."};
inline constexpr DiagnosticDefinition malformed_identifier{
    "ARGOS-P0002", Severity::error, "identificador malformado",
    "Nomes devem começar por letra, _ ou caractere Unicode identificador válido.",
    "Use, por exemplo, total, _index ou nomeUsuario."};
inline constexpr DiagnosticDefinition reserved_identifier{
    "ARGOS-P0003", Severity::error, "palavra reservada usada como identificador",
    "Palavras-chave da linguagem não podem nomear variáveis, funções ou parâmetros.",
    "Escolha outro nome ou use um identificador escapado entre crases."};
inline constexpr DiagnosticDefinition incomplete_keyword{
    "ARGOS-P0004", Severity::error, "palavra-chave incompleta ou desconhecida",
    "Uma palavra parecida com uma construção da linguagem não foi reconhecida.",
    "Use a grafia canônica, por exemplo function main() -> void { ... }."};
} // namespace parser

namespace compiler {
inline constexpr DiagnosticDefinition unsupported_lowering{
    "ARGOS-C0001", Severity::error, "construção ainda não é compilável",
    "O compilador entendeu o trecho, mas ainda não aprendeu a executá-lo.",
    "Por enquanto, use uma construção do subconjunto executável do MVP."};
inline constexpr DiagnosticDefinition invalid_entry_point{
    "ARGOS-C0002", Severity::error, "ponto de entrada inválido",
    "Um executável ArgosLang precisa de uma function main sem parâmetros.",
    "Declare function main() -> void ou function main() -> int."};
} // namespace compiler

namespace runtime {
inline constexpr DiagnosticDefinition execution_failure{
    "ARGOS-R0001", Severity::error, "erro durante a execução",
    "O programa tentou fazer uma conta que não pode terminar com segurança, como dividir por zero.",
    "Confira os valores usados nessa conta e veja se o divisor não é zero."};
} // namespace runtime

namespace warning {
inline constexpr DiagnosticDefinition legacy_null_keyword{
    "ARGOS-W0002", Severity::warning, "grafia legada de nil",
    "'null' continua aceito apenas para facilitar a migração de fontes antigas.",
    "Use 'nil' em código novo."};
} // namespace warning

namespace semantic {
constexpr DiagnosticDefinition semantic_error(std::string_view code,
                                              std::string_view title,
                                              std::string_view help) {
  return {code, Severity::error, title,
          "Uma parte do código não combina com o tipo de valor esperado.", help};
}
namespace symbols {
inline constexpr DiagnosticDefinition duplicate_declaration{"ARGOS-S0001", Severity::error, "declaração duplicada", "Um nome foi declarado mais de uma vez no mesmo escopo.", "Renomeie uma declaração ou remova a duplicada."};
inline constexpr DiagnosticDefinition invalid_ast_node{"ARGOS-S0002", Severity::error, "nó de AST inválido", "A construção não é aceita naquela posição semântica.", "Revise a estrutura da declaração ou bloco."};
inline constexpr DiagnosticDefinition unknown_declaration{"ARGOS-S0003", Severity::error, "declaração semântica desconhecida", "O analisador não reconhece essa declaração na fase atual.", "Use uma declaração suportada ou atualize o frontend."};
inline constexpr DiagnosticDefinition unknown_statement{"ARGOS-S0004", Severity::error, "instrução semântica desconhecida", "O analisador não reconhece essa instrução na fase atual.", "Use uma instrução suportada ou atualize o frontend."};
inline constexpr DiagnosticDefinition unknown_expression{"ARGOS-S0005", Severity::error, "expressão semântica desconhecida", "O analisador não reconhece essa expressão na fase atual.", "Use uma expressão suportada ou atualize o frontend."};
inline constexpr DiagnosticDefinition unknown_identifier{"ARGOS-S0300", Severity::error, "identificador não declarado", "O nome não está visível neste escopo.", "Declare-o antes de usar ou corrija a grafia."};
inline constexpr DiagnosticDefinition uninitialized_variable{"ARGOS-S0301", Severity::error, "variável usada antes da inicialização", "A variável não recebeu valor antes da leitura.", "Inicialize a variável antes de usá-la."};
inline constexpr DiagnosticDefinition type_used_as_value{"ARGOS-S0302", Severity::error, "tipo usado como valor", "Um nome de tipo não pode ser avaliado como expressão.", "Use uma variável ou uma construção explícita."};
} // namespace symbols
namespace declarations {
inline constexpr DiagnosticDefinition invalid_variable_type{"ARGOS-S0100", Severity::error, "tipo inválido para variável", "void e never não podem ser tipos de armazenamento.", "Use um tipo que represente valores."};
inline constexpr DiagnosticDefinition incompatible_initializer{"ARGOS-S0101", Severity::error, "inicializador incompatível", "O valor inicial não pode ser atribuído ao tipo declarado.", "Ajuste o tipo ou a expressão inicializadora."};
inline constexpr DiagnosticDefinition missing_variable_type_or_value{"ARGOS-S0103", Severity::error, "variável sem tipo e sem valor", "Não há informação suficiente para validar a declaração.", "Informe tipo e inicializador."};
inline constexpr DiagnosticDefinition uninferable_variable_type{"ARGOS-S0102", Severity::error, "tipo de variável não inferível", "O inicializador não permite deduzir um tipo seguro.", "Declare o tipo explicitamente."};
inline constexpr DiagnosticDefinition integer_literal_out_of_inferred_range{"ARGOS-S0104", Severity::error, "literal inteiro fora do intervalo inferido", "O literal não cabe em int, tipo padrão de inferência inteira.", "Declare um tipo inteiro maior ou ajuste o literal."};
} // namespace declarations
namespace operators {
inline constexpr auto unary_numeric = semantic_error("ARGOS-S0310", "operador unário aritmético exige número", "Use int, double ou outro tipo numérico não opcional.");
inline constexpr auto logical_not = semantic_error("ARGOS-S0311", "negação lógica exige bool", "Use ! apenas sobre uma expressão bool.");
inline constexpr auto arithmetic_operands = semantic_error("ARGOS-S0320", "operação aritmética exige números", "Use operandos numéricos não opcionais.");
inline constexpr auto arithmetic_type_mismatch = semantic_error("ARGOS-S0321", "operandos aritméticos incompatíveis", "Use tipos numéricos compatíveis.");
inline constexpr auto equality_type_mismatch = semantic_error("ARGOS-S0322", "operandos de igualdade incompatíveis", "Compare valores de tipos compatíveis.");
inline constexpr auto lifted_equality_requires_optional = semantic_error("ARGOS-S0323", "igualdade segura exige opcional ou nil", "Use ?== somente com opcionais ou nil.");
inline constexpr auto coalesce_requires_optional = semantic_error("ARGOS-S0324", "coalescência exige opcional", "Use ?? em um valor T?.");
inline constexpr auto coalesce_fallback_mismatch = semantic_error("ARGOS-S0325", "valor padrão incompatível", "O lado direito de ?? deve ser compatível com T.");
inline constexpr auto relational_operands = semantic_error("ARGOS-S0326", "comparação relacional exige números", "Compare valores numéricos não opcionais.");
inline constexpr auto logical_operands = semantic_error("ARGOS-S0327", "operador lógico exige bool", "Use && e || apenas com bool.");
inline constexpr auto relational_type_mismatch = semantic_error("ARGOS-S0328", "comparação relacional incompatível", "Use tipos numéricos compatíveis.");
inline constexpr auto lifted_equality_type_mismatch = semantic_error("ARGOS-S0329", "igualdade segura incompatível", "Compare opcionais com tipos-base compatíveis.");
} // namespace operators
namespace assignment {
inline constexpr auto immutable_target =
    semantic_error("ARGOS-S0330", "alvo não reatribuível",
                   "Use um alvo mutável e acesso direto.");
inline constexpr auto incompatible_value =
    semantic_error("ARGOS-S0331", "valor atribuído incompatível",
                   "Atribua valor compatível com o tipo do alvo.");
inline constexpr auto safe_source_requires_optional = semantic_error(
    "ARGOS-S0332", "?= exige fonte opcional", "Declare a fonte como T?.");
inline constexpr auto safe_value_mismatch =
    semantic_error("ARGOS-S0333", "valor desembrulhado incompatível",
                   "O tipo desembrulhado deve ser compatível com o alvo.");
inline constexpr auto coalescing_target_requires_optional =
    semantic_error("ARGOS-S0334", "coalescência exige alvo opcional",
                   "Use ?"
                   "?= com um alvo T?.");
inline constexpr auto coalescing_value_mismatch = semantic_error(
    "ARGOS-S0335",
    "valor de ?"
    "?= incompatível",
    "Atribua valor compatível com o alvo ou tipo desembrulhado.");
inline constexpr auto compound_requires_numeric =
    semantic_error("ARGOS-S0336", "atribuição aritmética exige números",
                   "Desembrulhe opcionais antes de fazer aritmética.");
inline constexpr auto compound_requires_initialized_target =
    semantic_error("ARGOS-S0337", "alvo composto não inicializado",
                   "Inicialize o alvo por atribuição simples antes de usá-lo.");
inline constexpr auto force_source = semantic_error(
    "ARGOS-S0338", "!!= exige fonte opcional", "Use uma fonte T?.");
inline constexpr auto force_type =
    semantic_error("ARGOS-S0339", "valor de !!= incompatível",
                   "O tipo desembrulhado deve caber no alvo.");
} // namespace assignment
namespace control {
inline constexpr DiagnosticDefinition return_outside_function{"ARGOS-S0200", Severity::error, "return fora de função", "return só pode encerrar uma function.", "Mova-o para o corpo de uma função."};
inline constexpr DiagnosticDefinition incompatible_return{"ARGOS-S0201", Severity::error, "retorno incompatível", "O valor retornado não corresponde ao tipo declarado.", "Retorne um valor compatível ou altere a assinatura."};
inline constexpr DiagnosticDefinition missing_return{"ARGOS-S0202", Severity::error, "retorno obrigatório ausente", "Uma função com retorno não-void pode terminar sem retornar.", "Retorne um valor em todos os caminhos."};
inline constexpr DiagnosticDefinition break_outside_loop{"ARGOS-S0203", Severity::error, "break fora de laço", "break só pode ser usado dentro de while ou futuro for.", "Remova-o ou coloque-o dentro de um laço."};
inline constexpr DiagnosticDefinition continue_outside_loop{"ARGOS-S0204", Severity::error, "continue fora de laço", "continue só pode ser usado dentro de while ou futuro for.", "Remova-o ou coloque-o dentro de um laço."};
} // namespace control
namespace types {
inline constexpr DiagnosticDefinition unknown_type{"ARGOS-S0400", Severity::error, "tipo não declarado", "O nome de tipo não foi encontrado.", "Use um tipo primitivo ou declare o tipo antes do uso."};
inline constexpr DiagnosticDefinition condition_not_boolean{"ARGOS-S0370", Severity::error, "condição não booleana", "if e while exigem uma expressão bool.", "Use uma comparação ou expressão booleana."};
inline constexpr auto invalid_lvalue = semantic_error(
    "ARGOS-S0380", "alvo não é um l-value",
    "Use uma variável, campo, propriedade ou elemento de array reatribuível.");
inline constexpr auto invalid_optional =
    semantic_error("ARGOS-S0401", "tipo opcional inválido",
                   "Use tipo habitável e até 4095 camadas opcionais.");
inline constexpr auto index_base = semantic_error(
    "ARGOS-S0375", "base de indexação incompatível",
    "Use [] em array presente ou ?[ em array com uma camada opcional.");
inline constexpr auto index_type =
    semantic_error("ARGOS-S0376", "índice incompatível",
                   "O índice deve ser int não opcional.");
inline constexpr auto array_type = semantic_error(
    "ARGOS-S0377", "tipo de array inválido",
    "Use elementos homogêneos habitáveis e anote arrays vazios ou só com nil.");
inline constexpr auto array_element =
    semantic_error("ARGOS-S0378", "elemento incompatível",
                   "Use elementos atribuíveis ao tipo declarado do array.");
} // namespace types
namespace members {
inline constexpr auto safe_access_requires_optional = semantic_error(
    "ARGOS-S0350", "?. exige uma camada opcional",
    "Use ?. em T?; desembrulhe camadas adicionais explicitamente.");
inline constexpr auto unsafe_access_on_optional =
    semantic_error("ARGOS-S0351", "acesso direto em opcional",
                   "Use ?. ou desembrulhe com !!.");
inline constexpr auto member_base_not_nominal =
    semantic_error("ARGOS-S0352", "base sem membros nominais",
                   "Use um tipo que declare membros.");
inline constexpr auto unknown_member = semantic_error(
    "ARGOS-S0353", "membro não declarado", "Corrija o nome do membro.");
} // namespace members
namespace postfix {
inline constexpr auto propagate_requires_optional =
    semantic_error("ARGOS-S0360", "propagação exige opcional",
                   "O operador ? pós-fixo não é suportado.");
inline constexpr auto unwrap_requires_optional = semantic_error(
    "ARGOS-S0361", "!! exige opcional", "Use !! apenas em um valor T?.");
inline constexpr auto increment_requires_numeric =
    semantic_error("ARGOS-S0362", "incremento exige número não opcional",
                   "Desembrulhe antes de incrementar.");
inline constexpr auto increment_requires_mutable_target =
    semantic_error("ARGOS-S0363", "incremento exige alvo mutável",
                   "Use variável numérica mutável.");
inline constexpr auto unsupported_propagation =
    semantic_error("ARGOS-S0364", "propagação pós-fixa indisponível",
                   "Use ?? ou !! para consumir opcionais.");
inline constexpr auto invalid_postfix_not =
    semantic_error("ARGOS-S0365", "! pós-fixo inválido",
                   "Use ! para negação ou !! para desembrulhar.");
} // namespace postfix
namespace protocols {
inline constexpr auto duplicate_protocol_component = semantic_error("ARGOS-S0500", "componente de protocolo duplicado", "Cada componente precisa ter nome único.");
inline constexpr auto invalid_protocol_component_type = semantic_error("ARGOS-S0501", "tipo de componente inválido", "Use um tipo de armazenamento válido.");
inline constexpr auto incompatible_protocol_default = semantic_error("ARGOS-S0502", "valor padrão de protocolo incompatível", "Use um valor compatível com o tipo do componente.");
} // namespace protocols
} // namespace semantic

namespace semantic::calls {
inline constexpr DiagnosticDefinition non_named_callee{
    "ARGOS-S0340", Severity::error, "alvo de chamada não é uma função nomeada",
    "Esta versão só resolve chamadas cujo alvo é um identificador de função.",
    "Declare uma function com nome e chame-a diretamente."};
inline constexpr DiagnosticDefinition undeclared_function{
    "ARGOS-S0341", Severity::error, "função não declarada",
    "O identificador usado na chamada não resolve para uma função visível.",
    "Declare a função antes de chamá-la ou corrija o nome."};
inline constexpr DiagnosticDefinition missing_argument{
    "ARGOS-S0342", Severity::error, "argumento obrigatório ausente",
    "Um parâmetro da função não recebeu argumento.",
    "Forneça o argumento exigido pela assinatura."};
inline constexpr DiagnosticDefinition incompatible_argument{
    "ARGOS-S0343", Severity::error, "tipo de argumento incompatível",
    "O valor passado não pode ser atribuído ao parâmetro correspondente.",
    "Passe um valor do tipo exigido pelo parâmetro."};
inline constexpr DiagnosticDefinition positional_argument_without_slot{
    "ARGOS-S0344", Severity::error, "argumento posicional sem parâmetro posicional",
    "A assinatura espera um rótulo neste ponto da chamada.",
    "Use o rótulo do parâmetro ou ajuste a ordem dos argumentos."};
inline constexpr DiagnosticDefinition unknown_argument_label{
    "ARGOS-S0345", Severity::error, "rótulo de argumento inexistente",
    "Nenhum parâmetro da função possui esse rótulo externo.",
    "Use um rótulo declarado na assinatura da função."};
inline constexpr DiagnosticDefinition duplicate_argument{
    "ARGOS-S0346", Severity::error, "parâmetro recebeu mais de um argumento",
    "O mesmo parâmetro foi preenchido por argumentos diferentes.",
    "Remova o argumento repetido ou corrija os rótulos."};
inline constexpr DiagnosticDefinition duplicate_parameter_label{
    "ARGOS-S0347", Severity::error, "rótulo de parâmetro duplicado",
    "Dois parâmetros da mesma função possuem o mesmo rótulo externo.",
    "Dê a cada parâmetro rotulado um nome externo único."};
} // namespace semantic::calls

namespace semantic::objects {
inline constexpr DiagnosticDefinition invalid_modifier{"ARGOS-S0600", Severity::error, "modificador incompatível", "O modificador não se aplica a esta declaração.", "Remova o modificador ou use uma declaração compatível."};
inline constexpr DiagnosticDefinition missing_body{"ARGOS-S0601", Severity::error, "corpo de função ausente", "Uma função concreta precisa de implementação.", "Adicione um corpo ou declare um requisito abstract."};
inline constexpr DiagnosticDefinition invalid_construction{"ARGOS-S0602", Severity::error, "tipo não pode ser instanciado", "new exige uma classe concreta.", "Construa uma implementação concreta do contrato."};
inline constexpr DiagnosticDefinition inheritance_cycle{"ARGOS-S0603", Severity::error, "ciclo de herança", "Um tipo depende de si mesmo na cadeia de bases.", "Remova a dependência circular."};
inline constexpr DiagnosticDefinition invalid_bases{"ARGOS-S0604", Severity::error, "bases incompatíveis", "Classes têm herança simples e interfaces só estendem interfaces.", "Use uma classe base e contratos distintos."};
inline constexpr DiagnosticDefinition final_base{"ARGOS-S0605", Severity::error, "classe base final", "Uma classe final não permite subclasses.", "Remova a herança ou revise o modificador final."};
inline constexpr DiagnosticDefinition incompatible_member{"ARGOS-S0606", Severity::error, "contrato de membro incompatível", "Membros herdados devem conservar assinatura e rótulos.", "Implemente o contrato com os mesmos tipos e rótulos."};
inline constexpr DiagnosticDefinition invalid_state{"ARGOS-S0607", Severity::error, "declaração de membro inválida", "Interfaces não armazenam estado e campos precisam de tipo.", "Declare o estado em uma classe com tipo explícito."};
inline constexpr DiagnosticDefinition invalid_override{"ARGOS-S0608", Severity::error, "sobrescrita inválida", "override exige um membro herdado que permita sobrescrita.", "Confira override, virtual, abstract, final e visibilidade."};
inline constexpr DiagnosticDefinition missing_implementation{"ARGOS-S0609", Severity::error, "requisito abstrato ausente", "Uma classe concreta deve implementar todos os contratos.", "Implemente o método ou declare a classe abstract."};
inline constexpr DiagnosticDefinition invalid_parameter{"ARGOS-S0610", Severity::error, "tipo de parâmetro inválido", "Parâmetros precisam de tipos habitáveis.", "Use um tipo diferente de void ou never."};
inline constexpr DiagnosticDefinition inaccessible_member{"ARGOS-S0611", Severity::error, "membro inacessível", "A visibilidade restringe o acesso ao membro.", "Use a API pública do tipo."};
inline constexpr DiagnosticDefinition lambda_annotation{"ARGOS-S0612", Severity::error, "tipo de lambda não inferível", "Esta lambda precisa de uma anotação explícita.", "Anote os parâmetros e, quando necessário, o retorno."};
inline constexpr DiagnosticDefinition invalid_capture{"ARGOS-S0613", Severity::error, "captura de lambda inválida", "Capturas exigem bindings locais imutáveis inicializados.", "Capture um binding var já inicializado; captura mutável ainda não está disponível."};
inline constexpr DiagnosticDefinition field_initialization{"ARGOS-S0614", Severity::error, "inicialização de campo inválida", "O estado da instância deve ser inicializado antes de ser utilizado.", "Inicialize cada campo em todos os caminhos de construct antes de retornar."};
} // namespace semantic::objects

namespace semantic {
inline const DiagnosticDefinition *
find_definition(std::string_view code) noexcept {
  static constexpr std::array definitions{
      &objects::invalid_modifier,
      &objects::missing_body,
      &objects::invalid_construction,
      &objects::inheritance_cycle,
      &objects::invalid_bases,
      &objects::final_base,
      &objects::incompatible_member,
      &objects::invalid_state,
      &objects::invalid_override,
      &objects::missing_implementation,
      &objects::invalid_parameter,
      &objects::inaccessible_member,
      &objects::lambda_annotation,
      &objects::invalid_capture,
      &objects::field_initialization,

      &symbols::duplicate_declaration,
      &symbols::invalid_ast_node,
      &symbols::unknown_declaration,
      &symbols::unknown_statement,
      &symbols::unknown_expression,
      &symbols::unknown_identifier,
      &symbols::uninitialized_variable,
      &symbols::type_used_as_value,
      &declarations::invalid_variable_type,
      &declarations::incompatible_initializer,
      &declarations::missing_variable_type_or_value,
      &declarations::uninferable_variable_type,
      &declarations::integer_literal_out_of_inferred_range,
      &operators::unary_numeric,
      &operators::logical_not,
      &operators::arithmetic_operands,
      &operators::arithmetic_type_mismatch,
      &operators::equality_type_mismatch,
      &operators::lifted_equality_requires_optional,
      &operators::coalesce_requires_optional,
      &operators::coalesce_fallback_mismatch,
      &operators::relational_operands,
      &operators::logical_operands,
      &operators::relational_type_mismatch,
      &operators::lifted_equality_type_mismatch,
      &assignment::force_source,
      &assignment::force_type,
      &assignment::immutable_target,
      &assignment::incompatible_value,
      &assignment::safe_source_requires_optional,
      &assignment::safe_value_mismatch,
      &assignment::coalescing_target_requires_optional,
      &assignment::coalescing_value_mismatch,
      &assignment::compound_requires_numeric,
      &assignment::compound_requires_initialized_target,
      &control::return_outside_function,
      &control::incompatible_return,
      &control::missing_return,
      &control::break_outside_loop,
      &control::continue_outside_loop,
      &types::unknown_type,
      &types::condition_not_boolean,
      &types::invalid_lvalue,
      &types::invalid_optional,
      &types::index_base,
      &types::index_type,
      &types::array_type,
      &types::array_element,
      &members::safe_access_requires_optional,
      &members::unsafe_access_on_optional,
      &members::member_base_not_nominal,
      &members::unknown_member,
      &postfix::propagate_requires_optional,
      &postfix::unwrap_requires_optional,
      &postfix::increment_requires_numeric,
      &postfix::increment_requires_mutable_target,
      &postfix::unsupported_propagation,
      &postfix::invalid_postfix_not,
      &protocols::duplicate_protocol_component,
      &protocols::invalid_protocol_component_type,
      &protocols::incompatible_protocol_default,
      &calls::non_named_callee,
      &calls::undeclared_function,
      &calls::missing_argument,
      &calls::incompatible_argument,
      &calls::positional_argument_without_slot,
      &calls::unknown_argument_label,
      &calls::duplicate_argument,
      &calls::duplicate_parameter_label,
  };
  for (const auto *definition : definitions)
    if (definition->code == code) return definition;
  return nullptr;
}
} // namespace semantic

} // namespace argos::diagnostics::catalog
