use proc_macro2::{Group, TokenStream};
use quote::{ToTokens, format_ident, quote};
use syn::{Expr, Ident, ItemConst, ItemFn, Token, parse, parse_macro_input};

/// Use as an attribute to annotate a parameterized test function that accepts a single parameter. The attribute must be given the name of a const array that is annotated with the `test_parameter` attribute. That const array will be used to supply parameter values to the test function.
#[proc_macro_attribute]
pub fn parameterized_test(
    input: proc_macro::TokenStream,
    annotated_item: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let macro_name = parse_macro_input!(input as Ident);
    let test = parse_macro_input!(annotated_item as ItemFn);

    let inner_func_name = test.sig.ident.clone();

    quote! {
        mod #inner_func_name {
            use super::*;

            #test

            #macro_name!{#inner_func_name}
        }
    }
    .into()
}

/// Use as an attribute to annotate a const array that will be used to supply the input values for a parameterized test.
///
/// # Panics
///
/// Panics if the annotated item is not a const array expression. Will also panic if the array contains a path expression that is somehow empty.
#[expect(clippy::expect_used, clippy::panic)]
#[proc_macro_attribute]
pub fn test_parameter(
    _input: proc_macro::TokenStream,
    annotated_item: proc_macro::TokenStream,
) -> proc_macro::TokenStream {
    let cloned_item_tokens = annotated_item.clone();
    let item = parse_macro_input!(cloned_item_tokens as ItemConst);

    let Expr::Array(array) = item.expr.as_ref() else {
        panic!("Expected expression to be an array");
    };

    let values: Vec<_> = array
        .elems
        .iter()
        .map(|n| {
            if let Expr::Path(path) = n {
                path.path
                    .segments
                    .last()
                    .expect("path expressions in the const array to have at least one segment")
                    .ident
                    .to_token_stream()
            } else {
                n.to_token_stream()
            }
        })
        .collect();

    let annotated_item = TokenStream::from(annotated_item);

    let const_item_name = item.ident;
    let macro_name = format_ident!("{}_macro", &const_item_name);

    let macro_output = quote! {
        macro_rules! #macro_name {
            ( $inner_test_name:ident ) => {
                array_parameterized_test::generate_tests!{
                    $inner_test_name,
                    #const_item_name,
                    [#(#values),*]
                }
            };
        }

        #[allow(unused_imports)]
        pub(crate) use #macro_name as #const_item_name;

        #annotated_item
    };

    macro_output.into()
}

struct GenerateTestsInput {
    inner_test_name: Ident,
    const_item_name: Ident,
    const_item_values: Group,
}

impl syn::parse::Parse for GenerateTestsInput {
    fn parse(input: parse::ParseStream) -> syn::Result<Self> {
        let inner_test_name = input.parse()?;
        let _: Token![,] = input.parse()?;
        let const_item_name = input.parse()?;
        let _: Token![,] = input.parse()?;
        let const_item_values = input.parse()?;

        Ok(GenerateTestsInput {
            inner_test_name,
            const_item_name,
            const_item_values,
        })
    }
}

/// A macro used to generate multiple test functions given a parameterized test function name, a const array identifier and the array's values.
#[proc_macro]
pub fn generate_tests(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let GenerateTestsInput {
        inner_test_name,
        const_item_name,
        const_item_values,
    } = parse_macro_input!(input as GenerateTestsInput);

    let tokens: proc_macro2::TokenStream = const_item_values
        .stream()
        .into_iter()
        .step_by(2)
        .enumerate()
        .flat_map(|(i, value)| {
            let suffix = value
                .to_string()
                .escape_default()
                .collect::<String>()
                .replace(|c: char| !c.is_ascii_alphanumeric(), "_");

            let test_name = format_ident!("_{suffix}");

            quote! {
                #[test]
                #[allow(non_snake_case)]
                fn #test_name() {
                    #inner_test_name(#const_item_name[#i]);
                }
            }
        })
        .collect();

    tokens.into()
}
